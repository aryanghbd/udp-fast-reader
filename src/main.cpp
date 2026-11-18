#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "udp_receiver.hpp"
#include "url.hpp"

namespace {

volatile std::sig_atomic_t g_stop = 0;

void on_signal(int) { g_stop = 1; }

void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options] <udp://host:port>\n"
              << "  -c, --count N     stop after N packets (default: unlimited)\n"
              << "  -t, --timeout MS  stop after MS ms with no packets (default: wait forever)\n"
              << "  -x, --hex         hex dump packet payloads\n"
              << "  -s, --size BYTES  receive buffer size (default: 65535)\n"
              << "  -h, --help        show this help\n";
}

void hex_dump(const char* data, std::size_t len) {
    for (std::size_t off = 0; off < len; off += 16) {
        std::printf("  %04zx ", off);
        const std::size_t end = std::min(len, off + 16);
        for (std::size_t i = off; i < off + 16; ++i) {
            if (i < end) std::printf(" %02x", static_cast<unsigned char>(data[i]));
            else std::printf("   ");
        }
        std::printf("  |");
        for (std::size_t i = off; i < end; ++i) {
            const unsigned char c = static_cast<unsigned char>(data[i]);
            std::putchar(c >= 32 && c < 127 ? c : '.');
        }
        std::printf("|\n");
    }
}

bool parse_num(const char* s, long min, long& out) {
    char* end = nullptr;
    const long v = std::strtol(s, &end, 10);
    if (!*s || *end || v < min) return false;
    out = v;
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    long count = 0;       // 0 = unlimited
    long timeout_ms = 0;  // 0 = wait forever
    long buf_size = 65535;
    bool hex = false;
    std::string url_arg;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next_num = [&](long min, long& out) {
            if (i + 1 >= argc || !parse_num(argv[++i], min, out)) {
                std::cerr << "invalid value for " << a << "\n";
                std::exit(2);
            }
        };
        if (a == "-h" || a == "--help") {
            usage(argv[0]);
            return 0;
        } else if (a == "-c" || a == "--count") {
            next_num(1, count);
        } else if (a == "-t" || a == "--timeout") {
            next_num(1, timeout_ms);
        } else if (a == "-s" || a == "--size") {
            next_num(1, buf_size);
        } else if (a == "-x" || a == "--hex") {
            hex = true;
        } else if (!a.empty() && a[0] == '-') {
            std::cerr << "unknown option: " << a << "\n";
            usage(argv[0]);
            return 2;
        } else if (url_arg.empty()) {
            url_arg = a;
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (url_arg.empty()) {
        usage(argv[0]);
        return 2;
    }

    try {
        const UdpUrl url = parse_udp_url(url_arg);
        UdpReceiver rx(url);

        struct sigaction sa{};
        sa.sa_handler = on_signal;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);

        std::vector<char> buf(static_cast<std::size_t>(buf_size));
        long received = 0;
        auto last_rx = std::chrono::steady_clock::now();
        constexpr int kPollSliceMs = 200;  // lets us notice g_stop and the idle timeout

        while (!g_stop && (count == 0 || received < count)) {
            const auto pkt = rx.receive(buf.data(), buf.size(), kPollSliceMs);
            const auto now = std::chrono::steady_clock::now();
            if (!pkt) {
                if (timeout_ms > 0 &&
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - last_rx).count() >=
                        timeout_ms) {
                    std::cerr << "timed out waiting for packets\n";
                    return received ? 0 : 1;
                }
                continue;
            }
            last_rx = now;
            ++received;
            std::printf("#%ld %s %zu bytes\n", received, pkt->source.c_str(), pkt->size);
            if (hex) hex_dump(buf.data(), pkt->size);
            std::fflush(stdout);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
