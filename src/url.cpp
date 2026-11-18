#include "url.hpp"

#include <stdexcept>

UdpUrl parse_udp_url(const std::string& url) {
    static const std::string scheme = "udp://";
    if (url.compare(0, scheme.size(), scheme) != 0) {
        throw std::invalid_argument("URL must start with udp://");
    }

    std::string rest = url.substr(scheme.size());
    const auto slash = rest.find_first_of("/?");
    if (slash != std::string::npos) rest.erase(slash);

    UdpUrl out;
    std::string port_str;

    if (!rest.empty() && rest.front() == '[') {
        const auto close = rest.find(']');
        if (close == std::string::npos || rest.compare(close + 1, 1, ":") != 0) {
            throw std::invalid_argument("expected [ipv6]:port");
        }
        out.host = rest.substr(1, close - 1);
        port_str = rest.substr(close + 2);
    } else {
        const auto colon = rest.rfind(':');
        if (colon == std::string::npos) throw std::invalid_argument("missing port");
        out.host = rest.substr(0, colon);
        port_str = rest.substr(colon + 1);
    }

    if (out.host.empty()) throw std::invalid_argument("missing host");
    if (port_str.empty() || port_str.size() > 5 ||
        port_str.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument("invalid port");
    }
    const unsigned long port = std::stoul(port_str);
    if (port == 0 || port > 65535) throw std::invalid_argument("port out of range");
    out.port = static_cast<std::uint16_t>(port);
    return out;
}
