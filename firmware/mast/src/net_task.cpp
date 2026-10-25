// UDP side: receives RC packets from the GCS, sends status + telemetry back to it.
// Arduino headers first: IPAddress.h clashes with lwIP's INADDR_NONE macro otherwise.
#include "config.h"
#include "shared.h"

#include <lwip/sockets.h>

namespace {

portMUX_TYPE rcMux = portMUX_INITIALIZER_UNLOCKED;
RcSnapshot rcLatest = {};

void handleRc(const uint8_t* buf, int len, int64_t now) {
    if (!wgcs::verify<wgcs::RcPacket>(buf, len, wgcs::MSG_RC)) {
        g_stats.rcDropped++;
        return;
    }
    wgcs::RcPacket pkt;
    memcpy(&pkt, buf, sizeof(pkt));

    RcSnapshot prev = rcLoad();
    bool idle = now - prev.rxTimeUs > cfg::SEQ_RESET_IDLE_US;
    if (prev.valid && !idle && (int16_t)(pkt.hdr.seq - prev.seq) <= 0) {
        g_stats.rcDropped++;  // duplicate or reordered
        return;
    }

    RcSnapshot rc;
    memcpy(rc.channels, pkt.channels, sizeof(rc.channels));
    rc.seq = pkt.hdr.seq;
    rc.gcsTimeUs = pkt.gcsTimeUs;
    rc.rxTimeUs = now;
    rc.valid = true;
    rcStore(rc);
    g_stats.rcReceived++;
}

void sendStatus(int sock, const sockaddr_in& to, uint16_t seq, int64_t now) {
    RcSnapshot rc = rcLoad();
    wgcs::MastStatusPacket pkt = {};
    pkt.uptimeMs = (uint32_t)(now / 1000);
    pkt.state = g_stats.state;
    pkt.lastRcSeq = rc.seq;
    pkt.echoGcsTimeUs = rc.gcsTimeUs;
    pkt.echoAgeUs = (uint32_t)(now - rc.rxTimeUs);
    pkt.rcReceived = g_stats.rcReceived;
    pkt.rcDropped = g_stats.rcDropped;
    pkt.failsafeCount = g_stats.failsafeCount;
    pkt.crsfFramesSent = g_stats.crsfFramesSent;
    pkt.crsfFramesReceived = g_stats.crsfFramesReceived;
    pkt.crsfCrcErrors = g_stats.crsfCrcErrors;
    pkt.timingRate = g_stats.timingRate;
    pkt.timingOffset = g_stats.timingOffset;
    wgcs::seal(pkt, wgcs::MSG_MAST_STATUS, seq);
    sendto(sock, &pkt, sizeof(pkt), 0, (const sockaddr*)&to, sizeof(to));
}

}  // namespace

void rcStore(const RcSnapshot& rc) {
    portENTER_CRITICAL(&rcMux);
    rcLatest = rc;
    portEXIT_CRITICAL(&rcMux);
}

RcSnapshot rcLoad() {
    portENTER_CRITICAL(&rcMux);
    RcSnapshot rc = rcLatest;
    portEXIT_CRITICAL(&rcMux);
    return rc;
}

void netTask(void*) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in local = {};
    local.sin_family = AF_INET;
    local.sin_port = htons(wgcs::MAST_PORT);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(sock, (const sockaddr*)&local, sizeof(local));

    timeval tv = {0, 10 * 1000};  // also bounds telemetry forwarding delay when RC is idle
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in gcs = {};
    bool haveGcs = false;
    uint16_t txSeq = 0;
    int64_t lastStatus = 0;
    uint8_t buf[256];

    for (;;) {
        sockaddr_in from = {};
        socklen_t fromLen = sizeof(from);
        int n = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
        int64_t now = esp_timer_get_time();

        if (n > 0 && wgcs::peekType(buf, n) == wgcs::MSG_RC) {
            handleRc(buf, n, now);
            gcs = from;  // reply wherever the GCS is sending from
            haveGcs = true;
        } else if (n > 0) {
            g_stats.rcDropped++;
        }
        if (!haveGcs) continue;

        TelemetryItem item;
        while (xQueueReceive(g_telemetryQueue, &item, 0) == pdTRUE) {
            wgcs::TelemetryPacket pkt = {};
            pkt.length = item.length;
            memcpy(pkt.frame, item.frame, item.length);
            wgcs::seal(pkt, wgcs::MSG_TELEMETRY, txSeq++);
            sendto(sock, &pkt, sizeof(pkt), 0, (const sockaddr*)&gcs, sizeof(gcs));
        }

        if (now - lastStatus >= cfg::STATUS_INTERVAL_US) {
            lastStatus = now;
            sendStatus(sock, gcs, txSeq++, now);
        }
    }
}
