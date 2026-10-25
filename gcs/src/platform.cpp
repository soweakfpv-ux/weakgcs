#include "platform.h"

#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <timeapi.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

using namespace std::chrono;

namespace platform {

#ifdef _WIN32

void init() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    timeBeginPeriod(1);
}

void shutdown() {
    timeEndPeriod(1);
    WSACleanup();
}

bool setRealtimePriority() { return SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL) != 0; }

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

// Coarse part of the wait. High-resolution waitable timers (Windows 10 1803+) are accurate to
// ~0.5 ms; older systems fall back to Sleep(), which timeBeginPeriod(1) makes ~1 ms accurate.
static void coarseSleep(steady_clock::duration d) {
    thread_local HANDLE timer =
        CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (timer) {
        LARGE_INTEGER due;
        due.QuadPart = -(LONGLONG)(duration_cast<nanoseconds>(d).count() / 100);  // relative, 100 ns units
        if (SetWaitableTimerEx(timer, &due, 0, nullptr, nullptr, nullptr, 0)) {
            WaitForSingleObject(timer, INFINITE);
            return;
        }
    }
    Sleep((DWORD)duration_cast<milliseconds>(d).count());
}

static constexpr auto SPIN_MARGIN = microseconds(700);

#else

void init() {}
void shutdown() {}

bool setRealtimePriority() {
    // Needs CAP_SYS_NICE or an rtprio limit, see docs/gcs.md.
    sched_param param{};
    param.sched_priority = 50;
    return pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) == 0;
}

static void coarseSleep(steady_clock::duration d) { std::this_thread::sleep_for(d); }

static constexpr auto SPIN_MARGIN = microseconds(150);

#endif

void sleepUntil(steady_clock::time_point deadline) {
    auto remaining = deadline - steady_clock::now();
    if (remaining > SPIN_MARGIN) coarseSleep(remaining - SPIN_MARGIN);
    while (steady_clock::now() < deadline) std::this_thread::yield();
}

}  // namespace platform
