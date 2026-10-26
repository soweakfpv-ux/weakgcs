#include "link.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "platform.h"

using namespace std::chrono;

namespace {

constexpr auto JOYSTICK_SCAN_INTERVAL = milliseconds(500);
constexpr auto MAST_TIMEOUT = milliseconds(500);
constexpr auto VRX_SEND_INTERVAL = milliseconds(250);
// Arm switch counts as "off" below this (ELRS/Betaflight arm on AUX1 high).
constexpr uint16_t ARM_OFF_BELOW = (uint16_t)crsf::usToTicks(1300);

bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                          [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); });
    return it != haystack.end();
}

}  // namespace

Link::Link(const Config& cfg, UdpSocket& sock) : cfg_(cfg), sock_(sock) {
    state_.armGuard = cfg.armChannel > 0;
    state_.vrxRequested = cfg.vrxChannel;
}

Link::~Link() { stop(); }

void Link::start() {
    running_ = true;
    rcThread_ = std::thread(&Link::rcThread, this);
    rxThread_ = std::thread(&Link::rxThread, this);
}

void Link::stop() {
    running_ = false;
    if (rcThread_.joinable()) rcThread_.join();
    if (rxThread_.joinable()) rxThread_.join();
    if (joystick_) SDL_CloseJoystick(joystick_);
    joystick_ = nullptr;
}

LinkSnapshot Link::snapshot() {
    std::lock_guard lock(mutex_);
    state_.mastAlive = lastStatusTime_ != Clock::time_point{} && Clock::now() - lastStatusTime_ < MAST_TIMEOUT;
    return state_;
}

void Link::setVrxChannel(uint8_t channel) {
    std::lock_guard lock(mutex_);
    state_.vrxRequested = channel;
    lastVrxSend_ = {};  // send right away
}

void Link::sendVrx(Clock::time_point now) {
    uint8_t channel;
    {
        std::lock_guard lock(mutex_);
        if (state_.vrxRequested == vrx::CHANNEL_NONE || now - lastVrxSend_ < VRX_SEND_INTERVAL) return;
        lastVrxSend_ = now;
        channel = state_.vrxRequested;
    }
    // Sent repeatedly so a rebooted mast re-tunes on its own; the mast ignores repeats.
    wgcs::VrxPacket pkt{};
    pkt.channel = channel;
    wgcs::seal(pkt, wgcs::MSG_VRX, vrxSeq_++);
    sock_.send(&pkt, sizeof(pkt));
}

uint32_t Link::nowUs() const { return (uint32_t)duration_cast<microseconds>(Clock::now() - epoch_).count(); }

void Link::updateJoystick(Clock::time_point now) {
    // SDL_HINT_AUTO_UPDATE_JOYSTICKS is off, so this thread drives joystick updates itself:
    // input is sampled right before each packet instead of once per rendered frame.
    SDL_UpdateJoysticks();

    if (joystick_ && !SDL_JoystickConnected(joystick_)) {
        std::printf("[rc] joystick disconnected\n");
        SDL_CloseJoystick(joystick_);
        joystick_ = nullptr;
        std::lock_guard lock(mutex_);
        state_.joystickConnected = false;
        state_.armGuard = cfg_.armChannel > 0;  // re-arm the guard: never resume straight into "armed"
    }
    if (joystick_ || now - lastJoystickScan_ < JOYSTICK_SCAN_INTERVAL) return;
    lastJoystickScan_ = now;

    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    for (int i = 0; ids && i < count && !joystick_; ++i) {
        const char* name = SDL_GetJoystickNameForID(ids[i]);
        std::string n = name ? name : "";
        if (!cfg_.joystickName.empty() && !containsIgnoreCase(n, cfg_.joystickName)) continue;
        joystick_ = SDL_OpenJoystick(ids[i]);
        if (joystick_) {
            std::printf("[rc] joystick: %s (%d axes, %d buttons)\n", n.c_str(), SDL_GetNumJoystickAxes(joystick_),
                        SDL_GetNumJoystickButtons(joystick_));
            std::lock_guard lock(mutex_);
            state_.joystickConnected = true;
            state_.joystickName = n;
        }
    }
    SDL_free(ids);
}

void Link::rcThread() {
    bool rt = platform::setRealtimePriority();
    if (!rt) std::printf("[rc] warning: could not get real-time thread priority (see docs/gcs.md)\n");
    {
        std::lock_guard lock(mutex_);
        state_.realtimePriority = rt;
    }

    const auto period = duration_cast<Clock::duration>(duration<double>(1.0 / cfg_.rcRateHz));
    auto next = Clock::now();
    auto rateWindowStart = next;
    int rateCount = 0;
    uint16_t seq = 0;
    JoystickState js;

    while (running_) {
        platform::sleepUntil(next);
        auto now = Clock::now();
        next += period;
        if (now - next > period * 10) next = now + period;  // we were suspended; don't burst

        sendVrx(now);  // video works even without the radio connected
        updateJoystick(now);
        if (!joystick_) continue;  // no radio -> no packets -> mast failsafes the drone

        js.axes.resize(SDL_GetNumJoystickAxes(joystick_));
        for (size_t i = 0; i < js.axes.size(); ++i) js.axes[i] = SDL_GetJoystickAxis(joystick_, (int)i);
        js.buttons.resize(SDL_GetNumJoystickButtons(joystick_));
        for (size_t i = 0; i < js.buttons.size(); ++i) js.buttons[i] = SDL_GetJoystickButton(joystick_, (int)i);

        wgcs::RcPacket pkt{};
        uint16_t channels[crsf::NUM_CHANNELS];
        applyChannelMap(cfg_.channelMap, js, channels);

        bool guard;
        {
            std::lock_guard lock(mutex_);
            if (state_.armGuard && channels[cfg_.armChannel - 1] < ARM_OFF_BELOW) {
                state_.armGuard = false;
                std::printf("[rc] arm switch seen off, arm guard released\n");
            }
            guard = state_.armGuard;
        }
        if (guard) channels[cfg_.armChannel - 1] = crsf::CHANNEL_MIN;

        std::memcpy(pkt.channels, channels, sizeof(channels));
        pkt.gcsTimeUs = nowUs();
        pkt.flags = guard ? wgcs::RC_FLAG_ARM_GUARD : 0;
        wgcs::seal(pkt, wgcs::MSG_RC, seq++);
        sock_.send(&pkt, sizeof(pkt));

        ++rateCount;
        double windowS = duration<double>(now - rateWindowStart).count();
        std::lock_guard lock(mutex_);
        std::memcpy(state_.channels, channels, sizeof(channels));
        if (windowS >= 1.0) {
            state_.rcRateHz = rateCount / windowS;
            rateCount = 0;
            rateWindowStart = now;
        }
    }
}

void Link::rxThread() {
    uint8_t buf[512];
    while (running_) {
        int n = sock_.recv(buf, sizeof(buf));  // times out periodically so we can exit
        if (n <= 0) continue;
        auto now = Clock::now();

        switch (wgcs::peekType(buf, n)) {
            case wgcs::MSG_MAST_STATUS: {
                if (!wgcs::verify<wgcs::MastStatusPacket>(buf, n, wgcs::MSG_MAST_STATUS)) break;
                wgcs::MastStatusPacket pkt;
                std::memcpy(&pkt, buf, sizeof(pkt));
                uint32_t rttUs = nowUs() - pkt.echoGcsTimeUs - pkt.echoAgeUs;
                std::lock_guard lock(mutex_);
                state_.mast = pkt;
                state_.rttMs = rttUs / 1000.0;
                lastStatusTime_ = now;
                break;
            }
            case wgcs::MSG_TELEMETRY: {
                if (!wgcs::verify<wgcs::TelemetryPacket>(buf, n, wgcs::MSG_TELEMETRY)) break;
                wgcs::TelemetryPacket pkt;
                std::memcpy(&pkt, buf, sizeof(pkt));
                std::lock_guard lock(mutex_);
                applyTelemetryFrame(state_.telemetry, pkt.frame, std::min<size_t>(pkt.length, sizeof(pkt.frame)), now);
                break;
            }
            default:
                break;
        }
    }
}
