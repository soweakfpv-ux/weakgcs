#include "osd.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

using namespace std::chrono;

namespace {

constexpr float CHAR = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;  // 8 px
constexpr auto TELEMETRY_STALE = milliseconds(1000);

struct Color {
    Uint8 r, g, b;
};
constexpr Color WHITE{235, 235, 235};
constexpr Color GREEN{90, 230, 110};
constexpr Color YELLOW{255, 210, 60};
constexpr Color RED{255, 70, 60};

class Painter {
public:
    explicit Painter(SDL_Renderer* r) : r_(r) {
        SDL_GetCurrentRenderOutputSize(r, &w_, &h_);
        scale_ = h_ >= 1000 ? 3.0f : 2.0f;
    }

    float cols() const { return w_ / scale_ / CHAR; }
    float rows() const { return h_ / scale_ / CHAR; }

    // Text with a dark shadow so it stays readable on any video. Coordinates in character cells.
    void text(float col, float row, Color c, const char* fmt, ...) {
        char buf[160];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        put(col, row, c, buf, scale_);
    }

    void textRight(float row, Color c, const std::string& s) { put(cols() - (float)s.size() - 1, row, c, s.c_str(), scale_); }

    void banner(float row, Color c, const std::string& s) {
        float big = scale_ * 2;
        float col = (w_ / big / CHAR - (float)s.size()) / 2;
        put(col, row, c, s.c_str(), big);
    }

    void bar(float col, float row, float widthCols, float fraction, Color c) {
        SDL_SetRenderScale(r_, scale_, scale_);
        SDL_FRect bg{col * CHAR, row * CHAR + 1, widthCols * CHAR, CHAR - 2};
        SDL_SetRenderDrawColor(r_, 0, 0, 0, 160);
        SDL_RenderFillRect(r_, &bg);
        SDL_FRect fg = bg;
        fg.w *= std::fmin(std::fmax(fraction, 0.0f), 1.0f);
        SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, 255);
        SDL_RenderFillRect(r_, &fg);
        SDL_SetRenderScale(r_, 1, 1);
    }

private:
    void put(float col, float row, Color c, const char* s, float scale) {
        SDL_SetRenderScale(r_, scale, scale);
        float x = col * CHAR, y = row * CHAR;
        SDL_SetRenderDrawColor(r_, 0, 0, 0, 255);
        SDL_RenderDebugText(r_, x + 1, y + 1, s);
        SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, 255);
        SDL_RenderDebugText(r_, x, y, s);
        SDL_SetRenderScale(r_, 1, 1);
    }

    SDL_Renderer* r_;
    int w_ = 0, h_ = 0;
    float scale_ = 2;
};

bool fresh(Clock::time_point t, Clock::time_point now) { return t != Clock::time_point{} && now - t < TELEMETRY_STALE; }

const char* mastStateName(uint8_t s) {
    switch (s) {
        case wgcs::MAST_WAITING_FOR_GCS: return "WAITING";
        case wgcs::MAST_ACTIVE: return "ACTIVE";
        case wgcs::MAST_FAILSAFE: return "FAILSAFE";
    }
    return "?";
}

int cellCount(const Config& cfg, float voltage) {
    if (cfg.batteryCells > 0) return cfg.batteryCells;
    return voltage > 0.5f ? (int)std::ceil(voltage / 4.35f) : 0;
}

}  // namespace

void drawOsd(SDL_Renderer* renderer, const Config& cfg, const OsdOptions& opt, const LinkSnapshot& link,
             const VideoStats& video) {
    if (!opt.visible) return;
    Painter p(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    const auto now = Clock::now();
    const TelemetryState& t = link.telemetry;
    std::vector<std::pair<Color, std::string>> warnings;

    // ---- top left: ground side (radio, mast) ----
    if (link.mastAlive) {
        Color c = link.mast.state == wgcs::MAST_ACTIVE ? GREEN : RED;
        p.text(1, 1, c, "MAST %s  RTT %.1fms", mastStateName(link.mast.state), link.rttMs);
        if (link.mast.state == wgcs::MAST_FAILSAFE) warnings.push_back({RED, "MAST FAILSAFE"});
    } else {
        p.text(1, 1, RED, "MAST --");
        warnings.push_back({RED, "NO MAST LINK"});
    }
    p.text(1, 2, link.joystickConnected ? WHITE : RED, "RC %.0fHz %s%s", link.rcRateHz,
           link.joystickConnected ? link.joystickName.c_str() : "NO RADIO", link.realtimePriority ? "" : " (no RT prio)");
    if (!link.joystickConnected) warnings.push_back({RED, "NO RADIO (USB HID)"});
    if (link.joystickConnected && link.armGuard) warnings.push_back({YELLOW, "ARM GUARD: DISARM ON RADIO"});

    // ---- top right: ELRS link ----
    uint32_t rate = link.mastAlive ? link.mast.timingRate : 0;
    if (fresh(t.linkTime, now)) {
        const crsf::LinkStatistics& l = t.link;
        Color c = l.uplinkLq < cfg.lqWarn ? (l.uplinkLq < cfg.lqWarn / 2 ? RED : YELLOW) : GREEN;
        char buf[96];
        std::snprintf(buf, sizeof(buf), "ELRS %uHz LQ %u RSSI -%u SNR %d", rate ? 10000000u / rate : 0, l.uplinkLq,
                      std::min(l.uplinkRssi1, l.uplinkRssi2), l.uplinkSnr);
        p.textRight(1, c, buf);
        std::snprintf(buf, sizeof(buf), "%dmW  DL LQ %u RSSI -%u", crsf::txPowerMw(l.uplinkTxPower), l.downlinkLq,
                      l.downlinkRssi);
        p.textRight(2, WHITE, buf);
        if (l.uplinkLq < cfg.lqWarn) warnings.push_back({c, "LOW LQ"});
    } else {
        p.textRight(1, RED, "ELRS NO TELEMETRY");
        if (link.mastAlive && link.mast.state == wgcs::MAST_ACTIVE) warnings.push_back({RED, "ELRS LINK LOST"});
    }

    // ---- bottom left: battery ----
    float bottom = p.rows() - 2;
    if (fresh(t.batteryTime, now)) {
        const crsf::Battery& b = t.battery;
        int cells = cellCount(cfg, b.voltage);
        float perCell = cells ? b.voltage / cells : 0;
        bool low = perCell > 0 && perCell < cfg.cellWarnVoltage;
        p.text(1, bottom - 1, low ? RED : WHITE, "%.1fV %dS %.2fV/c", b.voltage, cells, perCell);
        p.text(1, bottom, WHITE, "%.1fA %umAh", b.current, b.usedMah);
        if (low) warnings.push_back({RED, "LOW BATTERY"});
    }

    // ---- bottom right: flight mode, GPS, video ----
    if (fresh(t.flightModeTime, now)) p.textRight(bottom - 2, WHITE, t.flightMode);
    if (fresh(t.gpsTime, now)) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%u SAT %dm %.0fkm/h", t.gps.satellites, t.gps.altitudeM, t.gps.groundSpeedKmh);
        p.textRight(bottom - 1, WHITE, buf);
    }
    if (video.open) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "VID %dx%d %.0ffps", video.width, video.height, video.fps);
        p.textRight(bottom, WHITE, buf);
    } else {
        p.textRight(bottom, RED, "NO VIDEO");
    }

    // ---- channel monitor ----
    if (opt.channelMonitor) {
        for (size_t i = 0; i < crsf::NUM_CHANNELS; ++i) {
            float row = 5 + (float)i;
            int us = crsf::ticksToUs(link.channels[i]);
            bool arm = cfg.armChannel == (int)i + 1;
            p.text(1, row, arm ? YELLOW : WHITE, "CH%-2zu %4d", i + 1, us);
            p.bar(11, row, 20, (float)(link.channels[i] - crsf::CHANNEL_MIN) / (crsf::CHANNEL_MAX - crsf::CHANNEL_MIN),
                  arm ? YELLOW : GREEN);
        }
    }

    // ---- warnings, centered ----
    float row = 4;
    for (const auto& [color, msg] : warnings) {
        p.banner(row, color, msg);
        row += 1.5f;
    }
}
