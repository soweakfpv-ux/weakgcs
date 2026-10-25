// Drives the ELRS TX module: sends RC frames at the rate the module asks for, reads telemetry back.
//
// Wiring is half-duplex on a single pin (like a radio's JR bay): our TX goes through a 1 kOhm
// resistor, our RX sits directly on the line. We therefore also receive our own frames (echo);
// those are addressed to the module and are simply skipped.
#include "config.h"
#include "shared.h"

namespace {

HardwareSerial& crsfSerial = Serial1;
hw_timer_t* tickTimer = nullptr;
TaskHandle_t crsfTaskHandle = nullptr;

void IRAM_ATTR onTick() {
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(crsfTaskHandle, &woken);
    if (woken) portYIELD_FROM_ISR();
}

// Applies the module's timing hint gradually, so our frames land just before the module needs them.
class TimingSync {
public:
    void update(const crsf::Timing& t) {
        rate_ = t.rate;
        pendingUs_ = t.offset / 10;  // replace, don't accumulate: the module reports the current error
    }

    uint32_t nextPeriodUs() {
        uint32_t nominal = rate_ ? rate_ / 10 : cfg::DEFAULT_PERIOD_US;
        int32_t maxStep = (int32_t)nominal / 10;
        int32_t step = pendingUs_;
        if (step > maxStep) step = maxStep;
        if (step < -maxStep) step = -maxStep;
        pendingUs_ -= step;
        return nominal + step;
    }

private:
    uint32_t rate_ = 0;  // 0.1 us
    int32_t pendingUs_ = 0;
};

TimingSync timing;
crsf::Parser parser;

void readModule() {
    while (crsfSerial.available()) {
        if (!parser.feed((uint8_t)crsfSerial.read())) continue;
        if (parser.frame()[0] == crsf::ADDR_MODULE) continue;  // our own echo

        g_stats.crsfFramesReceived++;
        crsf::Timing t;
        if (parser.type() == crsf::TYPE_RADIO_ID && crsf::decodeTiming(parser.payload(), parser.payloadSize(), t)) {
            timing.update(t);
            g_stats.timingRate = t.rate;
            g_stats.timingOffset = t.offset;
            continue;
        }

        TelemetryItem item;
        item.length = (uint8_t)parser.size();
        memcpy(item.frame, parser.frame(), parser.size());
        xQueueSend(g_telemetryQueue, &item, 0);  // drop if the network side is behind
    }
    g_stats.crsfCrcErrors = parser.crcErrors();
}

void sendRc(int64_t now) {
    RcSnapshot rc = rcLoad();
    if (!rc.valid) {
        g_stats.state = wgcs::MAST_WAITING_FOR_GCS;
        return;
    }
    if (now - rc.rxTimeUs > cfg::FAILSAFE_TIMEOUT_US) {
        if (g_stats.state.exchange(wgcs::MAST_FAILSAFE) == wgcs::MAST_ACTIVE) g_stats.failsafeCount++;
        return;  // silence on the CRSF line -> ELRS link drops -> receiver failsafe
    }
    g_stats.state = wgcs::MAST_ACTIVE;

    uint8_t frame[crsf::RC_FRAME_SIZE];
    crsf::buildRcFrame(rc.channels, frame);
    crsfSerial.write(frame, sizeof(frame));
    g_stats.crsfFramesSent++;
}

}  // namespace

void crsfTask(void*) {
    crsfTaskHandle = xTaskGetCurrentTaskHandle();
    crsfSerial.setRxBufferSize(512);
    crsfSerial.begin(cfg::CRSF_BAUD, SERIAL_8N1, cfg::CRSF_RX_PIN, cfg::CRSF_TX_PIN);

    // 1 MHz hardware timer, auto-reload. Each tick re-programs the alarm for the next period.
    tickTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(tickTimer, &onTick, true);
    timerAlarmWrite(tickTimer, cfg::DEFAULT_PERIOD_US, true);
    timerAlarmEnable(tickTimer);

    for (;;) {
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10)) == 0) {
            readModule();  // timer not firing? keep the parser alive anyway
            continue;
        }
        timerAlarmWrite(tickTimer, timing.nextPeriodUs(), true);
        readModule();
        sendRc(esp_timer_get_time());
    }
}
