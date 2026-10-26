// GCS <-> mast UDP protocol. Shared by the GCS app and the mast firmware.
//
// - GCS sends RcPacket to the mast at a fixed rate (default 500 Hz).
// - The mast replies to the sender address/port of the latest RcPacket with:
//     * MastStatusPacket at 10 Hz,
//     * TelemetryPacket for every CRSF telemetry frame received from the ELRS module.
// - GCS sends VrxPacket at 4 Hz when it controls the video receiver channel (SteadyView over SPI).
//
// All fields are little-endian (both x86 and ESP32 are little-endian, so structs are sent as-is).
// See docs/protocol.md for details.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace wgcs {

constexpr uint32_t MAGIC = 0x53434757;  // "WGCS"
constexpr uint8_t VERSION = 1;
constexpr uint16_t MAST_PORT = 5770;

enum MsgType : uint8_t {
    MSG_RC = 1,
    MSG_TELEMETRY = 2,
    MSG_MAST_STATUS = 3,
    MSG_VRX = 4,
};

enum MastState : uint8_t {
    MAST_WAITING_FOR_GCS = 0,  // never received RC since boot: CRSF output idle
    MAST_ACTIVE = 1,           // forwarding RC to the ELRS module
    MAST_FAILSAFE = 2,         // RC stream timed out: CRSF output stopped, drone goes to failsafe
};

// RcPacket::flags
constexpr uint8_t RC_FLAG_ARM_GUARD = 0x01;  // GCS is holding the arm channel low (informational)

#pragma pack(push, 1)

struct Header {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t seq;
};

struct RcPacket {
    Header hdr;
    uint32_t gcsTimeUs;    // GCS clock, echoed back in MastStatusPacket for RTT measurement
    uint8_t flags;
    uint8_t reserved;
    uint16_t channels[16];  // CRSF ticks, 172..1811
    uint16_t crc;
};

struct TelemetryPacket {
    Header hdr;
    uint8_t length;    // number of valid bytes in frame
    uint8_t frame[64];  // raw CRSF frame, as received from the ELRS module
    uint16_t crc;
};

struct MastStatusPacket {
    Header hdr;
    uint32_t uptimeMs;
    uint8_t state;            // MastState
    uint8_t vrxChannel;       // channel index (weakgcs/vrx.h) last sent to the VRX, 0xFF = not set
    uint16_t lastRcSeq;
    uint32_t echoGcsTimeUs;   // gcsTimeUs of the latest RcPacket
    uint32_t echoAgeUs;       // how long ago (mast clock) that packet arrived
    uint32_t rcReceived;      // valid RcPackets since boot
    uint32_t rcDropped;       // bad CRC / stale / out-of-order
    uint32_t failsafeCount;   // how many times the mast entered failsafe
    uint32_t crsfFramesSent;
    uint32_t crsfFramesReceived;
    uint32_t crsfCrcErrors;
    uint32_t timingRate;      // 0.1 us, last value reported by the ELRS module (0 = none yet)
    int32_t timingOffset;     // 0.1 us
    uint16_t crc;
};

// Desired video receiver channel. Sent repeatedly; the mast only re-tunes when it changes.
struct VrxPacket {
    Header hdr;
    uint8_t channel;  // index into vrx::FREQUENCIES
    uint8_t reserved;
    uint16_t crc;
};

#pragma pack(pop)

static_assert(sizeof(Header) == 8, "protocol layout");
static_assert(sizeof(RcPacket) == 48, "protocol layout");
static_assert(sizeof(TelemetryPacket) == 75, "protocol layout");
static_assert(sizeof(MastStatusPacket) == 58, "protocol layout");
static_assert(sizeof(VrxPacket) == 12, "protocol layout");

// CRC-16/CCITT-FALSE
inline uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; ++b) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

// Every packet ends with a crc16 over all preceding bytes.
template <typename T>
inline void seal(T& pkt, MsgType type, uint16_t seq) {
    pkt.hdr.magic = MAGIC;
    pkt.hdr.version = VERSION;
    pkt.hdr.type = type;
    pkt.hdr.seq = seq;
    pkt.crc = crc16(reinterpret_cast<const uint8_t*>(&pkt), sizeof(T) - 2);
}

template <typename T>
inline bool verify(const void* data, size_t len, MsgType type) {
    if (len != sizeof(T)) return false;
    const T* pkt = static_cast<const T*>(data);
    if (pkt->hdr.magic != MAGIC || pkt->hdr.version != VERSION || pkt->hdr.type != type) return false;
    return pkt->crc == crc16(static_cast<const uint8_t*>(data), sizeof(T) - 2);
}

// Returns the packet type if `data` looks like one of ours, 0 otherwise.
inline uint8_t peekType(const void* data, size_t len) {
    if (len < sizeof(Header)) return 0;
    const Header* h = static_cast<const Header*>(data);
    return (h->magic == MAGIC && h->version == VERSION) ? h->type : 0;
}

}  // namespace wgcs
