// Tunes an SPI-controlled 5.8 GHz receiver module (original Skyzone SteadyView, RX5808-style)
// to the channel requested by the GCS. Three lines: CLK, DATA, CS (3.3 V logic).
//
// Bit-banged slowly (~10 kHz), like the goggles do; a re-tune takes a few milliseconds.
#include <weakgcs/vrx.h>

#include "config.h"
#include "shared.h"

namespace {

constexpr uint32_t HALF_BIT_US = 25;

void writeRegister(uint32_t word) {
    digitalWrite(cfg::VRX_CS_PIN, LOW);
    delayMicroseconds(4 * HALF_BIT_US);
    for (int i = 0; i < vrx::TRANSFER_BITS; ++i) {
        digitalWrite(cfg::VRX_DATA_PIN, word & 1);
        delayMicroseconds(HALF_BIT_US);
        digitalWrite(cfg::VRX_CLK_PIN, HIGH);
        delayMicroseconds(HALF_BIT_US);
        digitalWrite(cfg::VRX_CLK_PIN, LOW);
        delayMicroseconds(HALF_BIT_US);
        word >>= 1;
    }
    delayMicroseconds(4 * HALF_BIT_US);
    digitalWrite(cfg::VRX_DATA_PIN, HIGH);
    digitalWrite(cfg::VRX_CS_PIN, HIGH);
}

void tune(uint8_t channel) {
    uint32_t synthB = vrx::writeWord(vrx::REG_SYNTH_B, vrx::synthBData(vrx::FREQUENCIES[channel]));
    writeRegister(vrx::writeWord(vrx::REG_SYNTH_A, vrx::SYNTH_A_DATA));
    writeRegister(synthB);
}

}  // namespace

void vrxTask(void*) {
    pinMode(cfg::VRX_CLK_PIN, OUTPUT);
    pinMode(cfg::VRX_DATA_PIN, OUTPUT);
    pinMode(cfg::VRX_CS_PIN, OUTPUT);
    digitalWrite(cfg::VRX_CLK_PIN, LOW);
    digitalWrite(cfg::VRX_DATA_PIN, HIGH);
    digitalWrite(cfg::VRX_CS_PIN, HIGH);

    bool modeSet = false;
    for (;;) {
        uint8_t channel;
        if (xQueueReceive(g_vrxQueue, &channel, portMAX_DELAY) != pdTRUE) continue;
        if (channel >= vrx::NUM_CHANNELS || channel == g_stats.vrxChannel) continue;

        if (!modeSet) {
            // SteadyView: a long pulse on CLK (with CS high) selects "Mix" mode, its default
            // combining mode. Done once, before the first tune.
            digitalWrite(cfg::VRX_CLK_PIN, HIGH);
            vTaskDelay(pdMS_TO_TICKS(100));
            digitalWrite(cfg::VRX_CLK_PIN, LOW);
            vTaskDelay(pdMS_TO_TICKS(500));
            modeSet = true;
        }
        tune(channel);
        g_stats.vrxChannel = channel;

        char name[3];
        vrx::channelName(channel, name);
        Serial.printf("[vrx] tuned to %s (%u MHz)\n", name, vrx::FREQUENCIES[channel]);
    }
}
