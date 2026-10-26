// The control link: reads the radio (USB HID joystick), streams RC packets to the mast, and
// receives mast status + drone telemetry. Runs on two background threads, independent of the
// render loop, so a slow frame never delays stick input.
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include <weakgcs/protocol.h>

#include "config.h"
#include "telemetry.h"
#include "udp_socket.h"

struct SDL_Joystick;

struct LinkSnapshot {
    // Radio / RC output
    bool joystickConnected = false;
    std::string joystickName;
    bool armGuard = false;  // arm channel forced low until the radio is seen disarmed
    bool realtimePriority = false;
    uint16_t channels[crsf::NUM_CHANNELS] = {};
    double rcRateHz = 0;
    uint8_t vrxRequested = vrx::CHANNEL_NONE;  // what we ask the mast for; mast.vrxChannel = what it set

    // Mast
    bool mastAlive = false;
    wgcs::MastStatusPacket mast{};
    double rttMs = 0;

    // Drone (via ELRS telemetry)
    TelemetryState telemetry;
};

class Link {
public:
    Link(const Config& cfg, UdpSocket& sock);
    ~Link();

    void start();
    void stop();
    LinkSnapshot snapshot();

    // Changes the video receiver channel (vrx::CHANNEL_NONE = stop controlling it).
    void setVrxChannel(uint8_t channel);

private:
    void rcThread();
    void rxThread();
    void updateJoystick(Clock::time_point now);
    void sendVrx(Clock::time_point now);
    uint32_t nowUs() const;

    const Config& cfg_;
    UdpSocket& sock_;
    const Clock::time_point epoch_ = Clock::now();
    std::atomic<bool> running_{false};
    std::thread rcThread_, rxThread_;

    // rcThread only
    SDL_Joystick* joystick_ = nullptr;
    Clock::time_point lastJoystickScan_{};
    uint16_t vrxSeq_ = 0;

    std::mutex mutex_;  // guards everything below
    LinkSnapshot state_;
    Clock::time_point lastStatusTime_{};
    Clock::time_point lastVrxSend_{};
};
