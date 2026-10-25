// OS-specific bits that matter for latency: timer resolution, thread priority, precise sleeping.
#pragma once

#include <chrono>

namespace platform {

// Call once at startup (Winsock, 1 ms timer resolution on Windows).
void init();
void shutdown();

// Raises the calling thread to (soft) real-time priority. Returns false if the OS refused.
bool setRealtimePriority();

// Sleeps until `deadline` with ~0.1 ms accuracy, without burning a whole core.
void sleepUntil(std::chrono::steady_clock::time_point deadline);

}  // namespace platform
