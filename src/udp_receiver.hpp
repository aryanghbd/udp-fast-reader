#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "url.hpp"

struct Packet {
    std::size_t size = 0;  // bytes written to the caller's buffer
    std::string source;    // "ip:port" of the sender
};

// Binds a UDP socket to the URL's address; joins the group if it is multicast.
class UdpReceiver {
public:
    explicit UdpReceiver(const UdpUrl& url);
    ~UdpReceiver();

    UdpReceiver(const UdpReceiver&) = delete;
    UdpReceiver& operator=(const UdpReceiver&) = delete;

    // Waits up to timeout_ms for a datagram. Returns nullopt on timeout or
    // interruption by a signal. Throws std::runtime_error on socket errors.
    std::optional<Packet> receive(char* buf, std::size_t cap, int timeout_ms);

private:
    int fd_ = -1;
};
