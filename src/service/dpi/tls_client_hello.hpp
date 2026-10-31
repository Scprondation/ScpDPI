#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace scpdpi::dpi {

enum class TlsParseState { incomplete, invalid, complete };

struct TlsClientHello {
    std::string hostname;
    std::size_t sni_offset{};
    std::size_t sni_length{};
};

struct TlsParseResult {
    TlsParseState state{TlsParseState::incomplete};
    TlsClientHello hello;
};

[[nodiscard]] TlsParseResult parse_tls_client_hello(std::span<const std::uint8_t> stream);
[[nodiscard]] TlsParseResult parse_tls_client_hello_message(std::span<const std::uint8_t> handshake);

} // namespace scpdpi::dpi
