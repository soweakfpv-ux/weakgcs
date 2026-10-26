// State shared between the network task and the CRSF task.
#pragma once

#include <Arduino.h>
#include <atomic>
#include <weakgcs/crsf.h>
#include <weakgcs/protocol.h>

struct RcSnapshot {
    uint16_t channels[crsf::NUM_CHANNELS];
    uint16_t seq;
    uint32_t gcsTimeUs;
    int64_t rxTimeUs;  // esp_timer_get_time() when the packet arrived
    bool valid;        // at least one packet received since boot
};

// Latest RC values from the GCS (written by the network task, read by the CRSF task).
void rcStore(const RcSnapshot& rc);
RcSnapshot rcLoad();

struct Stats {
    std::atomic<uint8_t> state{wgcs::MAST_WAITING_FOR_GCS};
    std::atomic<uint32_t> rcReceived{0};
    std::atomic<uint32_t> rcDropped{0};
    std::atomic<uint32_t> failsafeCount{0};
    std::atomic<uint32_t> crsfFramesSent{0};
    std::atomic<uint32_t> crsfFramesReceived{0};
    std::atomic<uint32_t> crsfCrcErrors{0};
    std::atomic<uint32_t> timingRate{0};
    std::atomic<int32_t> timingOffset{0};
    std::atomic<uint8_t> vrxChannel{0xFF};  // last channel sent to the VRX module
};
extern Stats g_stats;

// CRSF telemetry frames from the module, forwarded to the GCS by the network task.
struct TelemetryItem {
    uint8_t length;
    uint8_t frame[crsf::MAX_FRAME_SIZE];
};
extern QueueHandle_t g_telemetryQueue;

// Channel requests from the GCS (length-1 queue, latest value wins).
extern QueueHandle_t g_vrxQueue;

void crsfTask(void*);
void netTask(void*);
void vrxTask(void*);
