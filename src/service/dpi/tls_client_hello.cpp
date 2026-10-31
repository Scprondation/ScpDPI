#include "tls_client_hello.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <vector>

namespace scpdpi::dpi {
namespace {

std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U) |
                                      static_cast<std::uint16_t>(bytes[offset + 1U]));
}

std::uint32_t read_u24(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
           static_cast<std::uint32_t>(bytes[offset + 2U]);
}

bool has_bytes(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t length) {
    return offset <= bytes.size() && length <= bytes.size() - offset;
}

TlsParseResult invalid() { return {TlsParseState::invalid, {}}; }

TlsParseResult parse_sni(std::span<const std::uint8_t> handshake,
                         std::span<const std::size_t> stream_offsets,
                         std::size_t hello_size) {
    constexpr std::size_t handshake_header = 4U;
    constexpr std::size_t client_hello_header = 2U + 32U;
    const auto body = handshake.subspan(handshake_header, hello_size - handshake_header);
    std::size_t cursor = client_hello_header;
    if (!has_bytes(body, cursor, 1U)) return invalid();
    const std::size_t session_length = body[cursor++];
    if (!has_bytes(body, cursor, session_length + 2U)) return invalid();
    cursor += session_length;
    const std::size_t cipher_length = read_u16(body, cursor);
    cursor += 2U;
    if (cipher_length == 0U || (cipher_length % 2U) != 0U || !has_bytes(body, cursor, cipher_length + 1U)) return invalid();
    cursor += cipher_length;
    const std::size_t compression_length = body[cursor++];
    if (compression_length == 0U || !has_bytes(body, cursor, compression_length + 2U)) return invalid();
    cursor += compression_length;
    const std::size_t extensions_length = read_u16(body, cursor);
    cursor += 2U;
    if (!has_bytes(body, cursor, extensions_length)) return invalid();
    const std::size_t extensions_end = cursor + extensions_length;

    while (cursor < extensions_end) {
        if (!has_bytes(body, cursor, 4U)) return invalid();
        const std::uint16_t type = read_u16(body, cursor);
        const std::size_t length = read_u16(body, cursor + 2U);
        cursor += 4U;
        if (!has_bytes(body, cursor, length) || cursor + length > extensions_end) return invalid();
        if (type == 0U) {
            if (length < 5U) return invalid();
            const std::size_t list_length = read_u16(body, cursor);
            std::size_t name_cursor = cursor + 2U;
            const std::size_t name_end = name_cursor + list_length;
            if (list_length + 2U > length || name_end > cursor + length) return invalid();
            while (name_cursor < name_end) {
                if (!has_bytes(body, name_cursor, 3U) || name_cursor + 3U > name_end) return invalid();
                const std::uint8_t name_type = body[name_cursor++];
                const std::size_t name_length = read_u16(body, name_cursor);
                name_cursor += 2U;
                if (!has_bytes(body, name_cursor, name_length) || name_cursor + name_length > name_end) return invalid();
                if (name_type == 0U && name_length > 0U && name_length <= 253U) {
                    std::string hostname;
                    hostname.reserve(name_length);
                    for (std::size_t i = 0; i < name_length; ++i) {
                        const char ch = static_cast<char>(body[name_cursor + i]);
                        const auto byte = static_cast<unsigned char>(ch);
                        if (std::isalnum(byte) == 0 && ch != '-' && ch != '.') return invalid();
                        hostname.push_back(static_cast<char>(std::tolower(byte)));
                    }
                    if (name_cursor >= stream_offsets.size() || name_length > stream_offsets.size() - name_cursor) return invalid();
                    const std::size_t hello_offset = name_cursor + handshake_header;
                    return {TlsParseState::complete,
                            {std::move(hostname), stream_offsets[hello_offset],
                             stream_offsets[hello_offset + name_length - 1U] - stream_offsets[hello_offset] + 1U}};
                }
                name_cursor += name_length;
            }
        }
        cursor += length;
    }
    return {TlsParseState::complete, {}};
}

} // namespace

TlsParseResult parse_tls_client_hello(std::span<const std::uint8_t> stream) {
    if (stream.empty()) return {TlsParseState::incomplete, {}};
    if (stream.front() != 0x16U) return invalid();

    std::vector<std::uint8_t> handshake;
    std::vector<std::size_t> stream_offsets;
    std::size_t cursor = 0U;
    std::size_t expected_handshake_size = 0U;
    constexpr std::size_t max_client_hello = 65535U;

    while (true) {
        if (!has_bytes(stream, cursor, 5U)) return {TlsParseState::incomplete, {}};
        if (stream[cursor] != 0x16U || stream[cursor + 1U] != 0x03U) return invalid();
        const std::size_t record_length = read_u16(stream, cursor + 3U);
        if (record_length == 0U || record_length > 18432U) return invalid();
        const std::size_t record_payload = cursor + 5U;
        if (!has_bytes(stream, record_payload, record_length)) return {TlsParseState::incomplete, {}};
        if (handshake.size() > max_client_hello - record_length) return invalid();
        handshake.insert(handshake.end(), stream.begin() + static_cast<std::ptrdiff_t>(record_payload),
                         stream.begin() + static_cast<std::ptrdiff_t>(record_payload + record_length));
        for (std::size_t i = 0; i < record_length; ++i) stream_offsets.push_back(record_payload + i);
        cursor = record_payload + record_length;

        if (handshake.size() >= 4U) {
            if (handshake[0] != 0x01U) return invalid();
            const std::size_t body_length = read_u24(handshake, 1U);
            if (body_length > max_client_hello - 4U) return invalid();
            expected_handshake_size = 4U + body_length;
            if (handshake.size() >= expected_handshake_size)
                return parse_sni(handshake, stream_offsets, expected_handshake_size);
        }
    }
}

} // namespace scpdpi::dpi
