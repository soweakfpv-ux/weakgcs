// CRSF (Crossfire / ExpressLRS serial protocol) helpers.
// Header-only, no allocations, shared by the mast firmware (ESP32) and the GCS app.
//
// Frame layout: [addr][len][type][payload ...][crc8]
//   len  = number of bytes after `len` (type + payload + crc)
//   crc8 = CRC-8/DVB-S2 over type + payload
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace crsf {

constexpr uint8_t ADDR_FLIGHT_CONTROLLER = 0xC8;
constexpr uint8_t ADDR_RADIO = 0xEA;   // handset (that's us, from the module's point of view)
constexpr uint8_t ADDR_MODULE = 0xEE;  // ELRS TX module

constexpr uint8_t TYPE_GPS = 0x02;
constexpr uint8_t TYPE_BATTERY = 0x08;
constexpr uint8_t TYPE_LINK_STATISTICS = 0x14;
constexpr uint8_t TYPE_RC_CHANNELS_PACKED = 0x16;
constexpr uint8_t TYPE_ATTITUDE = 0x1E;
constexpr uint8_t TYPE_FLIGHT_MODE = 0x21;
constexpr uint8_t TYPE_RADIO_ID = 0x3A;
constexpr uint8_t RADIO_ID_SUBTYPE_TIMING = 0x10;  // "OpenTX sync": module tells handset when to send

constexpr size_t MAX_FRAME_SIZE = 64;
constexpr size_t NUM_CHANNELS = 16;
constexpr size_t RC_FRAME_SIZE = 26;  // addr + len + type + 22 payload + crc

// Channel values in CRSF "ticks". 172 = 988us, 992 = 1500us, 1811 = 2012us.
constexpr uint16_t CHANNEL_MIN = 172;
constexpr uint16_t CHANNEL_MID = 992;
constexpr uint16_t CHANNEL_MAX = 1811;

inline constexpr int ticksToUs(int ticks) { return (ticks - CHANNEL_MID) * 5 / 8 + 1500; }
inline constexpr int usToTicks(int us) { return (us - 1500) * 8 / 5 + CHANNEL_MID; }

inline uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0xD5) : (uint8_t)(crc << 1);
    }
    return crc;
}

// Builds an RC_CHANNELS_PACKED frame addressed to the TX module. `out` must hold RC_FRAME_SIZE bytes.
inline size_t buildRcFrame(const uint16_t ch[NUM_CHANNELS], uint8_t* out) {
    out[0] = ADDR_MODULE;
    out[1] = RC_FRAME_SIZE - 2;
    out[2] = TYPE_RC_CHANNELS_PACKED;
    uint8_t* p = out + 3;
    uint32_t bits = 0;
    int nbits = 0;
    size_t idx = 0;
    for (size_t i = 0; i < NUM_CHANNELS; ++i) {
        uint16_t v = ch[i];
        if (v < CHANNEL_MIN) v = CHANNEL_MIN;
        if (v > CHANNEL_MAX) v = CHANNEL_MAX;
        bits |= (uint32_t)v << nbits;
        nbits += 11;
        while (nbits >= 8) {
            p[idx++] = (uint8_t)bits;
            bits >>= 8;
            nbits -= 8;
        }
    }
    out[RC_FRAME_SIZE - 1] = crc8(out + 2, RC_FRAME_SIZE - 3);
    return RC_FRAME_SIZE;
}

// Unpacks the 22-byte RC payload into 16 channels.
inline void unpackChannels(const uint8_t* payload, uint16_t ch[NUM_CHANNELS]) {
    uint32_t bits = 0;
    int nbits = 0;
    size_t idx = 0;
    for (size_t i = 0; i < NUM_CHANNELS; ++i) {
        while (nbits < 11) {
            bits |= (uint32_t)payload[idx++] << nbits;
            nbits += 8;
        }
        ch[i] = bits & 0x7FF;
        bits >>= 11;
        nbits -= 11;
    }
}

// Streaming byte parser. Feed bytes one by one; when feed() returns true, frame()/size() hold
// a complete frame with a valid CRC.
class Parser {
public:
    bool feed(uint8_t b) {
        if (pos_ == 0) {
            if (b != ADDR_RADIO && b != ADDR_MODULE && b != ADDR_FLIGHT_CONTROLLER) return false;
        } else if (pos_ == 1) {
            if (b < 2 || b > MAX_FRAME_SIZE - 2) {
                pos_ = 0;
                return false;
            }
        }
        buf_[pos_++] = b;
        if (pos_ < 2 || pos_ < (size_t)buf_[1] + 2) return false;

        size_t size = pos_;
        pos_ = 0;
        if (crc8(buf_ + 2, size - 3) != buf_[size - 1]) {
            ++crcErrors_;
            return false;
        }
        size_ = size;
        return true;
    }

    void reset() { pos_ = 0; }
    const uint8_t* frame() const { return buf_; }
    size_t size() const { return size_; }
    uint8_t type() const { return buf_[2]; }
    const uint8_t* payload() const { return buf_ + 3; }
    size_t payloadSize() const { return size_ - 4; }
    uint32_t crcErrors() const { return crcErrors_; }

private:
    uint8_t buf_[MAX_FRAME_SIZE] = {};
    size_t pos_ = 0;
    size_t size_ = 0;
    uint32_t crcErrors_ = 0;
};

// ---- Telemetry decoding (all multi-byte fields are big-endian) ----

inline uint16_t be16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
inline uint32_t be24(const uint8_t* p) { return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2]; }
inline uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

struct LinkStatistics {
    uint8_t uplinkRssi1;   // -dBm
    uint8_t uplinkRssi2;   // -dBm
    uint8_t uplinkLq;      // %
    int8_t uplinkSnr;      // dB
    uint8_t activeAntenna;
    uint8_t rfMode;
    uint8_t uplinkTxPower;  // enum, see txPowerMw()
    uint8_t downlinkRssi;   // -dBm
    uint8_t downlinkLq;     // %
    int8_t downlinkSnr;     // dB
};

inline bool decodeLinkStatistics(const uint8_t* p, size_t n, LinkStatistics& out) {
    if (n < 10) return false;
    out = {p[0], p[1], p[2], (int8_t)p[3], p[4], p[5], p[6], p[7], p[8], (int8_t)p[9]};
    return true;
}

inline int txPowerMw(uint8_t idx) {
    static const int16_t table[] = {0, 10, 25, 100, 500, 1000, 2000, 250, 50};
    return idx < sizeof(table) / sizeof(table[0]) ? table[idx] : -1;
}

struct Battery {
    float voltage;  // V
    float current;  // A
    uint32_t usedMah;
    uint8_t remainingPct;
};

inline bool decodeBattery(const uint8_t* p, size_t n, Battery& out) {
    if (n < 8) return false;
    out.voltage = be16(p) / 10.0f;
    out.current = be16(p + 2) / 10.0f;
    out.usedMah = be24(p + 4);
    out.remainingPct = p[7];
    return true;
}

struct Gps {
    double latitude;   // deg
    double longitude;  // deg
    float groundSpeedKmh;
    float headingDeg;
    int altitudeM;
    uint8_t satellites;
};

inline bool decodeGps(const uint8_t* p, size_t n, Gps& out) {
    if (n < 15) return false;
    out.latitude = (int32_t)be32(p) / 1e7;
    out.longitude = (int32_t)be32(p + 4) / 1e7;
    out.groundSpeedKmh = be16(p + 8) / 10.0f;
    out.headingDeg = be16(p + 10) / 100.0f;
    out.altitudeM = (int)be16(p + 12) - 1000;
    out.satellites = p[14];
    return true;
}

struct Attitude {
    float pitchRad;
    float rollRad;
    float yawRad;
};

inline bool decodeAttitude(const uint8_t* p, size_t n, Attitude& out) {
    if (n < 6) return false;
    out.pitchRad = (int16_t)be16(p) / 10000.0f;
    out.rollRad = (int16_t)be16(p + 2) / 10000.0f;
    out.yawRad = (int16_t)be16(p + 4) / 10000.0f;
    return true;
}

// Copies the null-terminated flight mode string ("ACRO", "!ERR", "WAIT"...).
inline bool decodeFlightMode(const uint8_t* p, size_t n, char* out, size_t outSize) {
    if (n == 0 || outSize == 0) return false;
    size_t len = 0;
    while (len < n && len + 1 < outSize && p[len] != 0) ++len;
    memcpy(out, p, len);
    out[len] = 0;
    return true;
}

// Module timing hint. Values in units of 0.1 us.
struct Timing {
    uint32_t rate;   // packet interval the module wants (e.g. 20000 = 2 ms = 500 Hz)
    int32_t offset;  // how far our frames are from the ideal moment; positive = send later
};

// RADIO_ID frames are extended frames: payload = [dest][origin][subtype][...]
inline bool decodeTiming(const uint8_t* p, size_t n, Timing& out) {
    if (n < 11 || p[2] != RADIO_ID_SUBTYPE_TIMING) return false;
    out.rate = be32(p + 3);
    out.offset = (int32_t)be32(p + 7);
    return true;
}

}  // namespace crsf
