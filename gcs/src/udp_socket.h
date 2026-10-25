// Minimal connected UDP socket (Winsock / BSD sockets).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    // Creates the socket and fixes the peer address. recv() blocks at most `recvTimeoutMs`.
    bool open(const std::string& ip, uint16_t port, int recvTimeoutMs, std::string& error);
    void close();

    // Both return bytes transferred, or -1 (error / timeout). Safe to call from two threads.
    int send(const void* data, size_t len);
    int recv(void* buf, size_t len);

private:
#ifdef _WIN32
    uintptr_t sock_ = ~(uintptr_t)0;
#else
    int sock_ = -1;
#endif
};
