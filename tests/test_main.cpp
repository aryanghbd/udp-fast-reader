#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "udp_receiver.hpp"
#include "url.hpp"

namespace {

int g_failures = 0;

#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                          \
        }                                                                          \
    } while (0)

bool rejects(const std::string& url) {
    try {
        parse_udp_url(url);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void test_parse_ipv4() {
    const auto u = parse_udp_url("udp://127.0.0.1:9999");
    CHECK(u.host == "127.0.0.1");
    CHECK(u.port == 9999);
}

void test_parse_hostname_and_ignores_query() {
    const auto u = parse_udp_url("udp://example.com:5000/path?x=1");
    CHECK(u.host == "example.com");
    CHECK(u.port == 5000);
}

void test_parse_ipv6() {
    const auto u = parse_udp_url("udp://[::1]:1234");
    CHECK(u.host == "::1");
    CHECK(u.port == 1234);
}

void test_parse_multicast() {
    const auto u = parse_udp_url("udp://239.1.1.1:5000");
    CHECK(u.host == "239.1.1.1");
    CHECK(u.port == 5000);
}

void test_parse_rejects_bad_input() {
    CHECK(rejects("http://127.0.0.1:80"));
    CHECK(rejects("127.0.0.1:80"));
    CHECK(rejects("udp://127.0.0.1"));
    CHECK(rejects("udp://:80"));
    CHECK(rejects("udp://host:abc"));
    CHECK(rejects("udp://host:0"));
    CHECK(rejects("udp://host:65536"));
    CHECK(rejects("udp://host:"));
    CHECK(rejects("udp://[::1]80"));
    CHECK(rejects("udp://[::1:80"));
}

void test_receive_times_out_when_idle() {
    UdpReceiver rx(UdpUrl{"127.0.0.1", 47311});
    char buf[64];
    CHECK(!rx.receive(buf, sizeof buf, 50).has_value());
}

void test_receive_loopback_packet() {
    constexpr std::uint16_t port = 47312;
    UdpReceiver rx(UdpUrl{"127.0.0.1", port});

    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &dst.sin_addr);
    const char msg[] = "hello";
    sendto(fd, msg, sizeof msg - 1, 0, reinterpret_cast<sockaddr*>(&dst), sizeof dst);
    close(fd);

    char buf[64];
    const auto pkt = rx.receive(buf, sizeof buf, 1000);
    CHECK(pkt.has_value());
    if (pkt) {
        CHECK(pkt->size == 5);
        CHECK(std::memcmp(buf, "hello", 5) == 0);
        CHECK(pkt->source.rfind("127.0.0.1:", 0) == 0);
    }
}

void test_unresolvable_host_throws() {
    bool threw = false;
    try {
        UdpReceiver rx(UdpUrl{"no-such-host.invalid", 47313});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
}

}  // namespace

int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        {"parse_ipv4", test_parse_ipv4},
        {"parse_hostname_and_ignores_query", test_parse_hostname_and_ignores_query},
        {"parse_ipv6", test_parse_ipv6},
        {"parse_multicast", test_parse_multicast},
        {"parse_rejects_bad_input", test_parse_rejects_bad_input},
        {"receive_times_out_when_idle", test_receive_times_out_when_idle},
        {"receive_loopback_packet", test_receive_loopback_packet},
        {"unresolvable_host_throws", test_unresolvable_host_throws},
    };
    for (const auto& [name, fn] : tests) {
        std::printf("[ RUN ] %s\n", name);
        fn();
    }
    if (g_failures) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("all tests passed\n");
    return 0;
}
