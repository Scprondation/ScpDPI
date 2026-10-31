#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace scpdpi::dpi {

/** Extracts the SNI from a decryptable QUIC v1/v2 client Initial packet. */
[[nodiscard]] std::optional<std::string> quic_initial_server_name(std::span<const std::uint8_t> datagram);

} // namespace scpdpi::dpi
