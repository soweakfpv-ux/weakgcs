#include "udp_socket.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#ifndef SIO_UDP_CONNRESET  // missing from MinGW headers
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

UdpSocket::~UdpSocket() { close(); }

bool UdpSocket::open(const std::string& ip, uint16_t port, int recvTimeoutMs, std::string& error) {
    close();
    sockaddr_in peer{};
    peer.sin_family = AF_INET;
    peer.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &peer.sin_addr) != 1) {
        error = "invalid IP address: " + ip;
        return false;
    }

    auto s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
    if (s == INVALID_SOCKET) {
        error = "socket() failed";
        return false;
    }
    sock_ = (uintptr_t)s;
    DWORD tv = (DWORD)recvTimeoutMs;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    // Don't let ICMP "port unreachable" (mast not up yet) turn into recv() errors.
    BOOL reportReset = FALSE;
    DWORD bytes = 0;
    WSAIoctl(s, SIO_UDP_CONNRESET, &reportReset, sizeof(reportReset), nullptr, 0, &bytes, nullptr, nullptr);
#else
    if (s < 0) {
        error = "socket() failed";
        return false;
    }
    sock_ = s;
    timeval tv{recvTimeoutMs / 1000, (recvTimeoutMs % 1000) * 1000};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int tos = 0xB8;  // DSCP EF (expedited forwarding); harmless on a direct cable
    setsockopt(s, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
#endif

    if (connect(s, (const sockaddr*)&peer, sizeof(peer)) != 0) {
        error = "connect() failed";
        close();
        return false;
    }
    return true;
}

void UdpSocket::close() {
#ifdef _WIN32
    if (sock_ != ~(uintptr_t)0) closesocket((SOCKET)sock_);
    sock_ = ~(uintptr_t)0;
#else
    if (sock_ >= 0) ::close(sock_);
    sock_ = -1;
#endif
}

int UdpSocket::send(const void* data, size_t len) {
#ifdef _WIN32
    return ::send((SOCKET)sock_, (const char*)data, (int)len, 0);
#else
    return (int)::send(sock_, data, len, 0);
#endif
}

int UdpSocket::recv(void* buf, size_t len) {
#ifdef _WIN32
    return ::recv((SOCKET)sock_, (char*)buf, (int)len, 0);
#else
    return (int)::recv(sock_, buf, len, 0);
#endif
}
