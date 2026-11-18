#pragma once

#include <cstdint>
#include <string>

struct UdpUrl {
    std::string host;  // hostname or IP literal, without IPv6 brackets
    std::uint16_t port = 0;
};

// Parses udp://host:port or udp://[ipv6]:port. Throws std::invalid_argument.
UdpUrl parse_udp_url(const std::string& url);
