#include "telemetry.h"

bool applyTelemetryFrame(TelemetryState& s, const uint8_t* frame, size_t len, Clock::time_point now) {
    if (len < 4 || len > crsf::MAX_FRAME_SIZE || (size_t)frame[1] + 2 != len) return false;
    if (crsf::crc8(frame + 2, len - 3) != frame[len - 1]) return false;

    const uint8_t type = frame[2];
    const uint8_t* p = frame + 3;
    const size_t n = len - 4;
    bool ok = false;
    switch (type) {
        case crsf::TYPE_LINK_STATISTICS:
            if ((ok = crsf::decodeLinkStatistics(p, n, s.link))) s.linkTime = now;
            break;
        case crsf::TYPE_BATTERY:
            if ((ok = crsf::decodeBattery(p, n, s.battery))) s.batteryTime = now;
            break;
        case crsf::TYPE_GPS:
            if ((ok = crsf::decodeGps(p, n, s.gps))) s.gpsTime = now;
            break;
        case crsf::TYPE_ATTITUDE:
            if ((ok = crsf::decodeAttitude(p, n, s.attitude))) s.attitudeTime = now;
            break;
        case crsf::TYPE_FLIGHT_MODE: {
            char mode[32];
            if ((ok = crsf::decodeFlightMode(p, n, mode, sizeof(mode)))) {
                s.flightMode = mode;
                s.flightModeTime = now;
            }
            break;
        }
        default:
            break;
    }
    if (ok) ++s.frames;
    return ok;
}
