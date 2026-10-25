// Unit tests for the protocol, CRSF encoding and the platform-independent GCS code.
// No framework: each CHECK prints the failing expression; exit code = number of failures.
#include <cstdio>
#include <cstring>
#include <string>

#include <weakgcs/crsf.h>
#include <weakgcs/protocol.h>

#include "channel_map.h"
#include "config.h"
#include "telemetry.h"

static int failures = 0;

#define CHECK(expr)                                                        \
    do {                                                                   \
        if (!(expr)) {                                                     \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

static void testCrc() {
    const char* s = "123456789";
    CHECK(crsf::crc8((const uint8_t*)s, 9) == 0xBC);     // CRC-8/DVB-S2 check value
    CHECK(wgcs::crc16((const uint8_t*)s, 9) == 0x29B1);  // CRC-16/CCITT-FALSE check value
}

static void testRcFrameRoundTrip() {
    uint16_t ch[16], back[16];
    for (int i = 0; i < 16; ++i) ch[i] = (uint16_t)(crsf::CHANNEL_MIN + i * 100);
    ch[15] = crsf::CHANNEL_MAX;
    uint8_t frame[crsf::RC_FRAME_SIZE];
    CHECK(crsf::buildRcFrame(ch, frame) == crsf::RC_FRAME_SIZE);
    CHECK(frame[0] == crsf::ADDR_MODULE);
    CHECK(frame[1] == 24);
    CHECK(frame[2] == crsf::TYPE_RC_CHANNELS_PACKED);
    crsf::unpackChannels(frame + 3, back);
    CHECK(std::memcmp(ch, back, sizeof(ch)) == 0);

    // All channels centered: known reference payload (992 = 0x3E0 packed 16 times).
    for (auto& c : ch) c = crsf::CHANNEL_MID;
    crsf::buildRcFrame(ch, frame);
    CHECK(frame[3] == 0xE0 && frame[4] == 0x03 && frame[5] == 0x1F);

    // Out-of-range values are clamped.
    ch[0] = 0;
    ch[1] = 2047;
    crsf::buildRcFrame(ch, frame);
    crsf::unpackChannels(frame + 3, back);
    CHECK(back[0] == crsf::CHANNEL_MIN && back[1] == crsf::CHANNEL_MAX);
}

static void testParser() {
    uint16_t ch[16];
    for (auto& c : ch) c = crsf::CHANNEL_MID;
    uint8_t frame[crsf::RC_FRAME_SIZE];
    crsf::buildRcFrame(ch, frame);

    crsf::Parser parser;
    const uint8_t noise[] = {0x00, 0x55, 0xEA, 0x01, 0xFF};  // includes a sync byte with an invalid length
    for (uint8_t b : noise) CHECK(!parser.feed(b));
    int complete = 0;
    for (uint8_t b : frame) complete += parser.feed(b);
    CHECK(complete == 1);
    CHECK(parser.size() == crsf::RC_FRAME_SIZE);
    CHECK(parser.type() == crsf::TYPE_RC_CHANNELS_PACKED);

    frame[10] ^= 0x01;  // corrupt
    complete = 0;
    for (uint8_t b : frame) complete += parser.feed(b);
    CHECK(complete == 0);
    CHECK(parser.crcErrors() == 1);
}

static size_t makeFrame(uint8_t type, const uint8_t* payload, size_t n, uint8_t* out) {
    out[0] = crsf::ADDR_RADIO;
    out[1] = (uint8_t)(n + 2);
    out[2] = type;
    std::memcpy(out + 3, payload, n);
    out[3 + n] = crsf::crc8(out + 2, n + 1);
    return n + 4;
}

static void testTelemetry() {
    TelemetryState s;
    auto now = Clock::now();
    uint8_t frame[64];

    const uint8_t link[] = {45, 50, 98, (uint8_t)-3, 1, 7, 3, 60, 100, 8};
    size_t n = makeFrame(crsf::TYPE_LINK_STATISTICS, link, sizeof(link), frame);
    CHECK(applyTelemetryFrame(s, frame, n, now));
    CHECK(s.link.uplinkRssi1 == 45 && s.link.uplinkLq == 98 && s.link.uplinkSnr == -3);
    CHECK(crsf::txPowerMw(s.link.uplinkTxPower) == 100);
    CHECK(s.linkTime == now);

    const uint8_t batt[] = {0x00, 0xA2, 0x00, 0x7B, 0x00, 0x01, 0xC2, 87};  // 16.2 V, 12.3 A, 450 mAh, 87 %
    n = makeFrame(crsf::TYPE_BATTERY, batt, sizeof(batt), frame);
    CHECK(applyTelemetryFrame(s, frame, n, now));
    CHECK(s.battery.voltage > 16.19f && s.battery.voltage < 16.21f);
    CHECK(s.battery.usedMah == 450 && s.battery.remainingPct == 87);

    const uint8_t mode[] = {'A', 'C', 'R', 'O', 0};
    n = makeFrame(crsf::TYPE_FLIGHT_MODE, mode, sizeof(mode), frame);
    CHECK(applyTelemetryFrame(s, frame, n, now));
    CHECK(s.flightMode == "ACRO");

    frame[n - 1] ^= 0xFF;  // bad CRC
    CHECK(!applyTelemetryFrame(s, frame, n, now));
    CHECK(s.frames == 3);
}

static void testTiming() {
    // Extended RADIO_ID frame: dest, origin, subtype, rate (be32), offset (be32)
    const uint8_t p[] = {0xEA, 0xEE, 0x10, 0x00, 0x00, 0x4E, 0x20, 0xFF, 0xFF, 0xFF, 0x9C};
    crsf::Timing t;
    CHECK(crsf::decodeTiming(p, sizeof(p), t));
    CHECK(t.rate == 20000);  // 2 ms = 500 Hz
    CHECK(t.offset == -100);
}

static void testProtocol() {
    wgcs::RcPacket pkt{};
    pkt.channels[4] = 1811;
    wgcs::seal(pkt, wgcs::MSG_RC, 1234);
    CHECK(wgcs::peekType(&pkt, sizeof(pkt)) == wgcs::MSG_RC);
    CHECK(wgcs::verify<wgcs::RcPacket>(&pkt, sizeof(pkt), wgcs::MSG_RC));
    CHECK(!wgcs::verify<wgcs::RcPacket>(&pkt, sizeof(pkt) - 1, wgcs::MSG_RC));
    CHECK(!wgcs::verify<wgcs::MastStatusPacket>(&pkt, sizeof(pkt), wgcs::MSG_MAST_STATUS));
    reinterpret_cast<uint8_t*>(&pkt)[20] ^= 0x40;
    CHECK(!wgcs::verify<wgcs::RcPacket>(&pkt, sizeof(pkt), wgcs::MSG_RC));
}

static void testChannelMap() {
    CHECK(axisToTicks(-32768) == crsf::CHANNEL_MIN);
    CHECK(axisToTicks(0) == crsf::CHANNEL_MID);
    CHECK(axisToTicks(32767) == crsf::CHANNEL_MAX);

    ChannelMap map;
    std::string err;
    CHECK(parseChannelMap("a1 -a0 b2 c172 u2000", map, err));
    JoystickState js;
    js.axes = {32767, -32768};
    js.buttons = {false, false, true};
    uint16_t out[16];
    applyChannelMap(map, js, out);
    CHECK(out[0] == crsf::CHANNEL_MIN);  // a1 = -32768
    CHECK(out[1] == crsf::CHANNEL_MIN);  // -a0 = inverted 32767
    CHECK(out[2] == crsf::CHANNEL_MAX);  // b2 pressed
    CHECK(out[3] == 172);
    CHECK(out[4] == crsf::usToTicks(2000));
    CHECK(out[5] == crsf::CHANNEL_MID);  // unmapped

    CHECK(!parseChannelMap("a0 x1", map, err));
    CHECK(!parseChannelMap("-b1", map, err));
    CHECK(!parseChannelMap("c5000", map, err));
    CHECK(!parseChannelMap("a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 a15 a16", map, err));
    // Missing axis on the device -> centered, not garbage.
    CHECK(parseChannelMap("a9", map, err));
    applyChannelMap(map, js, out);
    CHECK(out[0] == crsf::CHANNEL_MID);
}

static void testConfig() {
    Config cfg;
    std::string err;
    CHECK(parseConfig("[network]\nmast_ip = 10.0.0.5 ; comment\n[rc]\nrate_hz=250\nchannels = a0 a1\n"
                      "[video]\nvsync = yes\n",
                      cfg, err));
    CHECK(cfg.mastIp == "10.0.0.5");
    CHECK(cfg.rcRateHz == 250);
    CHECK(cfg.vsync);
    CHECK(cfg.channelMap[2].kind == ChannelSource::Constant);

    Config bad;
    CHECK(!parseConfig("[rc]\nrate_hz = 5000\n", bad, err));
    CHECK(!parseConfig("[rc]\nratehz = 500\n", bad, err));  // typo -> error, not silently ignored
    CHECK(err.find("line 2") != std::string::npos);
}

int main() {
    testCrc();
    testRcFrameRoundTrip();
    testParser();
    testTelemetry();
    testTiming();
    testProtocol();
    testChannelMap();
    testConfig();
    if (failures == 0) std::printf("all tests passed\n");
    return failures;
}
