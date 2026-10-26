#include <WiFi.h>

#include "config.h"
#include "shared.h"

Stats g_stats;
QueueHandle_t g_telemetryQueue;
QueueHandle_t g_vrxQueue;

namespace {

const char* stateName(uint8_t s) {
    switch (s) {
        case wgcs::MAST_WAITING_FOR_GCS: return "WAITING";
        case wgcs::MAST_ACTIVE: return "ACTIVE";
        case wgcs::MAST_FAILSAFE: return "FAILSAFE";
    }
    return "?";
}

void onNetworkEvent(arduino_event_id_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_CONNECTED: Serial.println("[eth] link up"); break;
        case ARDUINO_EVENT_ETH_DISCONNECTED: Serial.println("[eth] link down"); break;
        default: break;
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    Serial.println("\nweakgcs mast starting");

    g_telemetryQueue = xQueueCreate(32, sizeof(TelemetryItem));
    g_vrxQueue = xQueueCreate(1, sizeof(uint8_t));

    WiFi.onEvent(onNetworkEvent);
    ETH.begin(cfg::ETH_ADDR, cfg::ETH_POWER_PIN, cfg::ETH_MDC_PIN, cfg::ETH_MDIO_PIN, ETH_PHY_LAN8720, cfg::ETH_CLK);
    ETH.config(cfg::MAST_IP, cfg::GATEWAY, cfg::SUBNET);
    Serial.printf("[eth] static IP %s, UDP port %u\n", cfg::MAST_IP.toString().c_str(), wgcs::MAST_PORT);

    // Ethernet/lwIP live on core 0, so the timing-critical CRSF task gets core 1 to itself.
    xTaskCreatePinnedToCore(netTask, "net", 4096, nullptr, 5, nullptr, 0);
    xTaskCreatePinnedToCore(vrxTask, "vrx", 3072, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(crsfTask, "crsf", 4096, nullptr, configMAX_PRIORITIES - 2, nullptr, 1);
}

void loop() {
    static uint32_t lastSent = 0, lastRc = 0;
    uint32_t sent = g_stats.crsfFramesSent, rc = g_stats.rcReceived;
    uint32_t rate = g_stats.timingRate;
    Serial.printf("[mast] %s rc=%lu/s crsf=%lu/s telem=%lu drop=%lu fs=%lu crcerr=%lu elrs=%luHz off=%ldus\n",
                  stateName(g_stats.state), (unsigned long)(rc - lastRc), (unsigned long)(sent - lastSent),
                  (unsigned long)g_stats.crsfFramesReceived.load(), (unsigned long)g_stats.rcDropped.load(),
                  (unsigned long)g_stats.failsafeCount.load(), (unsigned long)g_stats.crsfCrcErrors.load(),
                  (unsigned long)(rate ? 10000000UL / rate : 0), (long)(g_stats.timingOffset / 10));
    lastSent = sent;
    lastRc = rc;
    delay(1000);
}
