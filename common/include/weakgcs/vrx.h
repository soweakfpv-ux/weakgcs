// 5.8 GHz analog video channels and the RTC6705 (RX5808-style) register encoding used to tune
// SPI-controlled receiver modules such as the original Skyzone SteadyView.
// Shared by the mast firmware (sends the registers) and the GCS (config, OSD).
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace vrx {

constexpr uint8_t NUM_BANDS = 6;
constexpr uint8_t CHANNELS_PER_BAND = 8;
constexpr uint8_t NUM_CHANNELS = NUM_BANDS * CHANNELS_PER_BAND;
constexpr uint8_t CHANNEL_NONE = 0xFF;

constexpr char BAND_NAMES[NUM_BANDS] = {'A', 'B', 'E', 'F', 'R', 'L'};

// MHz, index = band * 8 + (channel - 1)
constexpr uint16_t FREQUENCIES[NUM_CHANNELS] = {
    5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725,  // A (Boscam A)
    5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866,  // B (Boscam B)
    5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945,  // E (Boscam E)
    5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880,  // F (Fatshark / IRC)
    5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917,  // R (Raceband)
    5333, 5373, 5413, 5453, 5493, 5533, 5573, 5613,  // L (Lowrace, as in Betaflight/ELRS VTX tables)
};

// "R1", "f4" -> index; CHANNEL_NONE if invalid.
inline uint8_t parseChannel(const char* s) {
    if (!s || !s[0] || !s[1] || s[2]) return CHANNEL_NONE;
    char band = (s[0] >= 'a' && s[0] <= 'z') ? (char)(s[0] - 32) : s[0];
    if (s[1] < '1' || s[1] > '8') return CHANNEL_NONE;
    for (uint8_t b = 0; b < NUM_BANDS; ++b) {
        if (BAND_NAMES[b] == band) return (uint8_t)(b * CHANNELS_PER_BAND + (s[1] - '1'));
    }
    return CHANNEL_NONE;
}

// Writes e.g. "R1" (3 bytes incl. terminator).
inline void channelName(uint8_t index, char out[3]) {
    if (index >= NUM_CHANNELS) {
        out[0] = out[1] = '-';
    } else {
        out[0] = BAND_NAMES[index / CHANNELS_PER_BAND];
        out[1] = (char)('1' + index % CHANNELS_PER_BAND);
    }
    out[2] = 0;
}

// ---- RTC6705 SPI encoding ----
// A transfer is 25 bits, LSB first: 4-bit register address, 1 read/write bit, 20 data bits.
constexpr uint8_t REG_SYNTH_A = 0x00;  // reference divider
constexpr uint8_t REG_SYNTH_B = 0x01;  // N/A counters -> frequency
constexpr int TRANSFER_BITS = 25;
constexpr uint32_t SYNTH_A_DATA = 8;  // reference divider R = 8

inline constexpr uint32_t writeWord(uint8_t reg, uint32_t data) { return reg | (1u << 4) | (data << 5); }

// Local oscillator = 2 * (32 * N + A) MHz, IF = 479 MHz.
inline constexpr uint32_t synthBData(uint16_t mhz) {
    uint32_t x = (uint32_t)(mhz - 479) / 2;
    return ((x / 32) << 7) | (x % 32);
}

}  // namespace vrx
