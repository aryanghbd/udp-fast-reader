#include "udp_receiver.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace {

[[noreturn]] void fail(const std::string& what) {
    throw std::runtime_error(what + ": " + std::strerror(errno));
}

void join_multicast(int fd, const sockaddr* addr) {
    if (addr->sa_family == AF_INET) {
        const auto* a = reinterpret_cast<const sockaddr_in*>(addr);
        if (!IN_MULTICAST(ntohl(a->sin_addr.s_addr))) return;
        ip_mreq mreq{};
        mreq.imr_multiaddr = a->sin_addr;
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof mreq) < 0) {
            fail("join multicast group");
        }
    } else if (addr->sa_family == AF_INET6) {
        const auto* a = reinterpret_cast<const sockaddr_in6*>(addr);
        if (!IN6_IS_ADDR_MULTICAST(&a->sin6_addr)) return;
        ipv6_mreq mreq{};
        mreq.ipv6mr_multiaddr = a->sin6_addr;
        mreq.ipv6mr_interface = 0;
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof mreq) < 0) {
            fail("join multicast group");
        }
    }
}

std::string format_addr(const sockaddr_storage& ss, socklen_t len) {
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    if (getnameinfo(reinterpret_cast<const sockaddr*>(&ss), len, host, sizeof host, serv,
                    sizeof serv, NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
        return "?";
    }
    const bool v6 = ss.ss_family == AF_INET6;
    return std::string(v6 ? "[" : "") + host + (v6 ? "]:" : ":") + serv;
}

}  // namespace

UdpReceiver::UdpReceiver(const UdpUrl& url) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_NUMERICSERV;

    addrinfo* raw = nullptr;
    const int rc = getaddrinfo(url.host.c_str(), std::to_string(url.port).c_str(), &hints, &raw);
    if (rc != 0) throw std::runtime_error("resolve " + url.host + ": " + gai_strerror(rc));
    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> res(raw, freeaddrinfo);

    int last_errno = 0;
    for (const addrinfo* ai = res.get(); ai; ai = ai->ai_next) {
        const int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            last_errno = errno;
            continue;
        }
        const int on = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
#ifdef SO_REUSEPORT
        setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof on);
#endif
        if (bind(fd, ai->ai_addr, ai->ai_addrlen) < 0) {
            last_errno = errno;
            close(fd);
            continue;
        }
        try {
            join_multicast(fd, ai->ai_addr);
        } catch (...) {
            close(fd);
            throw;
        }
        fd_ = fd;
        return;
    }
    errno = last_errno;
    fail("bind " + url.host + ":" + std::to_string(url.port));
}

UdpReceiver::~UdpReceiver() {
    if (fd_ >= 0) close(fd_);
}

std::optional<Packet> UdpReceiver::receive(char* buf, std::size_t cap, int timeout_ms) {
    pollfd pfd{fd_, POLLIN, 0};
    const int pr = poll(&pfd, 1, timeout_ms);
    if (pr < 0) {
        if (errno == EINTR) return std::nullopt;
        fail("poll");
    }
    if (pr == 0) return std::nullopt;

    sockaddr_storage src{};
    socklen_t slen = sizeof src;
    const ssize_t n = recvfrom(fd_, buf, cap, 0, reinterpret_cast<sockaddr*>(&src), &slen);
    if (n < 0) {
        if (errno == EINTR || errno == EAGAIN) return std::nullopt;
        fail("recvfrom");
    }
    return Packet{static_cast<std::size_t>(n), format_addr(src, slen)};
}
