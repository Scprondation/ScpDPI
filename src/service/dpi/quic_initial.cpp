#include "quic_initial.hpp"

#include "tls_client_hello.hpp"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace scpdpi::dpi {
namespace {

constexpr std::uint32_t quic_v1 = 0x00000001U;
constexpr std::uint32_t quic_v2 = 0x6b3343cfU;
constexpr std::size_t max_crypto_stream = 65535U;
constexpr std::array<std::uint8_t, 20U> v1_initial_salt{
    0x38U, 0x76U, 0x2cU, 0xf7U, 0xf5U, 0x59U, 0x34U, 0xb3U, 0x4dU, 0x17U,
    0x9aU, 0xe6U, 0xa4U, 0xc8U, 0x0cU, 0xadU, 0xccU, 0xbbU, 0x7fU, 0x0aU
};
constexpr std::array<std::uint8_t, 20U> v2_initial_salt{
    0x0dU, 0xedU, 0xe3U, 0xdeU, 0xf7U, 0x00U, 0xa6U, 0xdbU, 0x81U, 0x93U,
    0x81U, 0xbeU, 0x6eU, 0x26U, 0x9dU, 0xcbU, 0xf9U, 0xbdU, 0x2eU, 0xd9U
};

struct QuicKeys {
    std::array<std::uint8_t, 16U> key{};
    std::array<std::uint8_t, 12U> iv{};
    std::array<std::uint8_t, 16U> header_key{};
};

bool succeeded(NTSTATUS status) { return status >= 0; }

std::uint16_t read_be16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8U) |
                                      static_cast<std::uint16_t>(data[1]));
}

std::uint32_t read_be32(const std::uint8_t* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24U) |
           (static_cast<std::uint32_t>(data[1]) << 16U) |
           (static_cast<std::uint32_t>(data[2]) << 8U) |
           static_cast<std::uint32_t>(data[3]);
}

bool read_varint(std::span<const std::uint8_t> data, std::size_t& cursor, std::uint64_t& value) {
    if (cursor >= data.size()) return false;
    const std::size_t length = std::size_t{1U} << (data[cursor] >> 6U);
    if (length > data.size() - cursor) return false;
    value = static_cast<std::uint64_t>(data[cursor] & 0x3fU);
    for (std::size_t index = 1U; index < length; ++index)
        value = (value << 8U) | static_cast<std::uint64_t>(data[cursor + index]);
    cursor += length;
    return true;
}

bool hmac_sha256(std::span<const std::uint8_t> key, std::span<const std::uint8_t> input,
                 std::array<std::uint8_t, 32U>& digest) {
    BCRYPT_ALG_HANDLE algorithm{};
    if (!succeeded(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                                BCRYPT_ALG_HANDLE_HMAC_FLAG))) return false;
    DWORD object_size{};
    DWORD returned{};
    bool ok = succeeded(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &returned, 0U));
    std::vector<std::uint8_t> object(object_size);
    BCRYPT_HASH_HANDLE hash{};
    if (ok) {
        ok = succeeded(BCryptCreateHash(algorithm, &hash, object.data(), object_size,
            const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0U));
    }
    if (ok) ok = succeeded(BCryptHashData(hash, const_cast<PUCHAR>(input.data()),
                                          static_cast<ULONG>(input.size()), 0U));
    if (ok) ok = succeeded(BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0U));
    if (hash != nullptr) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0U);
    return ok;
}

bool hkdf_expand(std::span<const std::uint8_t> secret, std::span<const std::uint8_t> info,
                 std::span<std::uint8_t> output) {
    std::array<std::uint8_t, 32U> previous{};
    std::size_t previous_size = 0U;
    std::size_t written = 0U;
    std::uint8_t counter = 1U;
    while (written < output.size()) {
        std::vector<std::uint8_t> input;
        input.reserve(previous_size + info.size() + 1U);
        input.insert(input.end(), previous.begin(), previous.begin() + static_cast<std::ptrdiff_t>(previous_size));
        input.insert(input.end(), info.begin(), info.end());
        input.push_back(counter);
        if (!hmac_sha256(secret, input, previous)) return false;
        previous_size = previous.size();
        const std::size_t count = std::min(previous_size, output.size() - written);
        std::copy_n(previous.begin(), count, output.begin() + static_cast<std::ptrdiff_t>(written));
        written += count;
        if (written < output.size() && counter == std::numeric_limits<std::uint8_t>::max()) return false;
        ++counter;
    }
    return true;
}

bool hkdf_expand_label(std::span<const std::uint8_t> secret, std::string_view label,
                       std::span<std::uint8_t> output) {
    constexpr std::string_view prefix = "tls13 ";
    if (prefix.size() + label.size() > std::numeric_limits<std::uint8_t>::max() ||
        output.size() > std::numeric_limits<std::uint16_t>::max()) return false;
    std::vector<std::uint8_t> info;
    info.reserve(2U + 1U + prefix.size() + label.size() + 1U);
    info.push_back(static_cast<std::uint8_t>(output.size() >> 8U));
    info.push_back(static_cast<std::uint8_t>(output.size()));
    info.push_back(static_cast<std::uint8_t>(prefix.size() + label.size()));
    info.insert(info.end(), prefix.begin(), prefix.end());
    info.insert(info.end(), label.begin(), label.end());
    info.push_back(0U);
    return hkdf_expand(secret, info, output);
}

bool derive_keys(std::span<const std::uint8_t> destination_id, bool version_two, QuicKeys& keys) {
    std::array<std::uint8_t, 32U> initial_secret{};
    std::array<std::uint8_t, 32U> client_secret{};
    const auto salt = version_two ? std::span<const std::uint8_t>(v2_initial_salt)
                                  : std::span<const std::uint8_t>(v1_initial_salt);
    if (!hmac_sha256(salt, destination_id, initial_secret) ||
        !hkdf_expand_label(initial_secret, "client in", client_secret)) return false;
    const std::string_view key_label = version_two ? "quicv2 key" : "quic key";
    const std::string_view iv_label = version_two ? "quicv2 iv" : "quic iv";
    const std::string_view hp_label = version_two ? "quicv2 hp" : "quic hp";
    return hkdf_expand_label(client_secret, key_label, keys.key) &&
           hkdf_expand_label(client_secret, iv_label, keys.iv) &&
           hkdf_expand_label(client_secret, hp_label, keys.header_key);
}

bool aes_ecb_encrypt(std::span<const std::uint8_t> key_bytes, std::span<const std::uint8_t, 16U> input,
                     std::array<std::uint8_t, 16U>& output) {
    BCRYPT_ALG_HANDLE algorithm{};
    if (!succeeded(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0U))) return false;
    bool ok = succeeded(BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE,
        reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_ECB)),
        static_cast<ULONG>(sizeof(BCRYPT_CHAIN_MODE_ECB)), 0U));
    DWORD object_size{};
    DWORD returned{};
    if (ok) ok = succeeded(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &returned, 0U));
    std::vector<std::uint8_t> object(object_size);
    BCRYPT_KEY_HANDLE key{};
    if (ok) ok = succeeded(BCryptGenerateSymmetricKey(algorithm, &key, object.data(), object_size,
        const_cast<PUCHAR>(key_bytes.data()), static_cast<ULONG>(key_bytes.size()), 0U));
    ULONG written{};
    if (ok) ok = succeeded(BCryptEncrypt(key, const_cast<PUCHAR>(input.data()), static_cast<ULONG>(input.size()),
        nullptr, nullptr, 0U, output.data(), static_cast<ULONG>(output.size()), &written, 0U)) && written == output.size();
    if (key != nullptr) BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(algorithm, 0U);
    return ok;
}

bool aes_gcm_decrypt(std::span<const std::uint8_t> key_bytes, std::span<const std::uint8_t, 12U> nonce,
                     std::span<const std::uint8_t> aad, std::span<const std::uint8_t> ciphertext,
                     std::span<const std::uint8_t, 16U> tag, std::vector<std::uint8_t>& plaintext) {
    BCRYPT_ALG_HANDLE algorithm{};
    if (!succeeded(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0U))) return false;
    bool ok = succeeded(BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE,
        reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
        static_cast<ULONG>(sizeof(BCRYPT_CHAIN_MODE_GCM)), 0U));
    DWORD object_size{};
    DWORD returned{};
    if (ok) ok = succeeded(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &returned, 0U));
    std::vector<std::uint8_t> object(object_size);
    BCRYPT_KEY_HANDLE key{};
    if (ok) ok = succeeded(BCryptGenerateSymmetricKey(algorithm, &key, object.data(), object_size,
        const_cast<PUCHAR>(key_bytes.data()), static_cast<ULONG>(key_bytes.size()), 0U));
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth{};
    auth.cbSize = sizeof(auth);
    auth.dwInfoVersion = BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO_VERSION;
    auth.pbNonce = const_cast<PUCHAR>(nonce.data());
    auth.cbNonce = static_cast<ULONG>(nonce.size());
    auth.pbAuthData = const_cast<PUCHAR>(aad.data());
    auth.cbAuthData = static_cast<ULONG>(aad.size());
    auth.pbTag = const_cast<PUCHAR>(tag.data());
    auth.cbTag = static_cast<ULONG>(tag.size());
    plaintext.resize(ciphertext.size());
    ULONG written{};
    if (ok) ok = succeeded(BCryptDecrypt(key, const_cast<PUCHAR>(ciphertext.data()),
        static_cast<ULONG>(ciphertext.size()), &auth, nullptr, 0U, plaintext.data(),
        static_cast<ULONG>(plaintext.size()), &written, 0U)) && written == plaintext.size();
    if (key != nullptr) BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(algorithm, 0U);
    return ok;
}

bool decrypt_initial(std::span<const std::uint8_t> datagram, std::size_t packet_number_offset,
                     std::size_t packet_end, const QuicKeys& keys, std::vector<std::uint8_t>& plaintext) {
    if (packet_number_offset > packet_end || packet_end - packet_number_offset < 4U + 16U) return false;
    std::array<std::uint8_t, 16U> sample{};
    std::copy_n(datagram.begin() + static_cast<std::ptrdiff_t>(packet_number_offset + 4U), sample.size(), sample.begin());
    std::array<std::uint8_t, 16U> mask{};
    if (!aes_ecb_encrypt(keys.header_key, sample, mask)) return false;
    std::vector<std::uint8_t> aad(datagram.begin(),
        datagram.begin() + static_cast<std::ptrdiff_t>(packet_number_offset + 4U));
    aad[0] ^= static_cast<std::uint8_t>(mask[0] & 0x0fU);
    const std::size_t packet_number_length = static_cast<std::size_t>((aad[0] & 0x03U) + 1U);
    if (packet_number_offset + packet_number_length > packet_end) return false;
    aad.resize(packet_number_offset + packet_number_length);
    std::uint64_t packet_number = 0U;
    for (std::size_t index = 0U; index < packet_number_length; ++index) {
        const auto byte = static_cast<std::uint8_t>(datagram[packet_number_offset + index] ^ mask[index + 1U]);
        aad[packet_number_offset + index] = byte;
        packet_number = (packet_number << 8U) | byte;
    }
    const std::size_t cipher_offset = packet_number_offset + packet_number_length;
    if (packet_end < cipher_offset || packet_end - cipher_offset < 16U) return false;
    const std::size_t cipher_size = packet_end - cipher_offset - 16U;
    std::array<std::uint8_t, 12U> nonce = keys.iv;
    for (std::size_t index = 0U; index < sizeof(packet_number); ++index) {
        const std::size_t nonce_index = nonce.size() - 1U - index;
        nonce[nonce_index] ^= static_cast<std::uint8_t>(packet_number >> (index * 8U));
    }
    const auto cipher = datagram.subspan(cipher_offset, cipher_size);
    const auto tag = std::span<const std::uint8_t, 16U>(datagram.data() + packet_end - 16U, 16U);
    return aes_gcm_decrypt(keys.key, nonce, aad, cipher, tag, plaintext);
}

std::optional<std::string> client_hello_sni(std::span<const std::uint8_t> plaintext) {
    std::size_t cursor = 0U;
    std::vector<std::uint8_t> crypto;
    while (cursor < plaintext.size()) {
        std::uint64_t frame_type{};
        if (!read_varint(plaintext, cursor, frame_type)) return std::nullopt;
        if (frame_type == 0x00U || frame_type == 0x01U) continue;
        if (frame_type == 0x06U) {
            std::uint64_t offset{};
            std::uint64_t length{};
            if (!read_varint(plaintext, cursor, offset) || !read_varint(plaintext, cursor, length) ||
                offset > max_crypto_stream || length > max_crypto_stream - static_cast<std::size_t>(offset) ||
                length > plaintext.size() - cursor) return std::nullopt;
            if (offset == crypto.size()) {
                crypto.insert(crypto.end(), plaintext.begin() + static_cast<std::ptrdiff_t>(cursor),
                              plaintext.begin() + static_cast<std::ptrdiff_t>(cursor + static_cast<std::size_t>(length)));
                const auto parsed = parse_tls_client_hello_message(crypto);
                if (parsed.state == TlsParseState::complete && !parsed.hello.hostname.empty())
                    return parsed.hello.hostname;
            }
            cursor += static_cast<std::size_t>(length);
            continue;
        }
        if (frame_type == 0x02U || frame_type == 0x03U) {
            std::uint64_t field{};
            for (std::size_t index = 0U; index < 4U; ++index)
                if (!read_varint(plaintext, cursor, field)) return std::nullopt;
            const std::uint64_t range_count = field;
            if (range_count > 256U) return std::nullopt;
            for (std::uint64_t index = 0U; index < range_count * 2U; ++index)
                if (!read_varint(plaintext, cursor, field)) return std::nullopt;
            if (frame_type == 0x03U)
                for (std::size_t index = 0U; index < 3U; ++index)
                    if (!read_varint(plaintext, cursor, field)) return std::nullopt;
            continue;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace

std::optional<std::string> quic_initial_server_name(std::span<const std::uint8_t> datagram) {
    if (datagram.size() < 7U || (datagram[0] & 0xc0U) != 0xc0U) return std::nullopt;
    const std::uint32_t version = read_be32(datagram.data() + 1U);
    const bool version_two = version == quic_v2;
    if (version != quic_v1 && !version_two) return std::nullopt;
    const auto packet_type = static_cast<std::uint8_t>((datagram[0] >> 4U) & 0x03U);
    if ((!version_two && packet_type != 0U) || (version_two && packet_type != 1U)) return std::nullopt;

    std::size_t cursor = 5U;
    const std::size_t destination_id_length = datagram[cursor++];
    if (destination_id_length > 20U || destination_id_length > datagram.size() - cursor) return std::nullopt;
    const auto destination_id = datagram.subspan(cursor, destination_id_length);
    cursor += destination_id_length;
    if (cursor >= datagram.size()) return std::nullopt;
    const std::size_t source_id_length = datagram[cursor++];
    if (source_id_length > 20U || source_id_length > datagram.size() - cursor) return std::nullopt;
    cursor += source_id_length;
    std::uint64_t token_length{};
    if (!read_varint(datagram, cursor, token_length) || token_length > datagram.size() - cursor) return std::nullopt;
    cursor += static_cast<std::size_t>(token_length);
    std::uint64_t packet_length{};
    if (!read_varint(datagram, cursor, packet_length) || packet_length > datagram.size() - cursor) return std::nullopt;
    const std::size_t packet_end = cursor + static_cast<std::size_t>(packet_length);

    QuicKeys keys;
    if (!derive_keys(destination_id, version_two, keys)) return std::nullopt;
    std::vector<std::uint8_t> plaintext;
    if (!decrypt_initial(datagram, cursor, packet_end, keys, plaintext)) return std::nullopt;
    return client_hello_sni(plaintext);
}

} // namespace scpdpi::dpi

#else
namespace scpdpi::dpi {
std::optional<std::string> quic_initial_server_name(std::span<const std::uint8_t>) { return std::nullopt; }
} // namespace scpdpi::dpi
#endif
