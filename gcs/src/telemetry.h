// Drone telemetry decoded from the CRSF frames the mast forwards.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

#include <weakgcs/crsf.h>

using Clock = std::chrono::steady_clock;

struct TelemetryState {
    crsf::LinkStatistics link{};
    Clock::time_point linkTime{};  // default (epoch) = never received
    crsf::Battery battery{};
    Clock::time_point batteryTime{};
    crsf::Gps gps{};
    Clock::time_point gpsTime{};
    crsf::Attitude attitude{};
    Clock::time_point attitudeTime{};
    std::string flightMode;
    Clock::time_point flightModeTime{};
    uint32_t frames = 0;
};

// Returns false if the frame is malformed or of a type we don't use.
bool applyTelemetryFrame(TelemetryState& state, const uint8_t* frame, size_t len, Clock::time_point now);
