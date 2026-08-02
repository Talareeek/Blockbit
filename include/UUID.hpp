#ifndef UUID_HPP
#define UUID_HPP

#include <array>
#include <cstdint>
#include <format>
#include <string>
#include <random>
#include <memory>
#include <cstring>

struct UUID
{
    std::array<uint8_t, 16> bytes;

    bool operator==(const UUID& other) const
    {
        return bytes == other.bytes;
    }

    std::string toString() const
    {
        return std::format(
            "{:02x}{:02x}{:02x}{:02x}-"
            "{:02x}{:02x}-"
            "{:02x}{:02x}-"
            "{:02x}{:02x}-"
            "{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
            bytes[0], bytes[1], bytes[2], bytes[3],
            bytes[4], bytes[5],
            bytes[6], bytes[7],
            bytes[8], bytes[9],
            bytes[10], bytes[11], bytes[12],
            bytes[13], bytes[14], bytes[15]);
    }
};

namespace std
{
    template<>
    struct hash<UUID>
    {
        size_t operator()(const UUID& uuid) const noexcept
        {
            size_t hash = 14695981039346656037ull;

            for (uint8_t b : uuid.bytes)
            {
                hash ^= b;
                hash *= 1099511628211ull;
            }

            return hash;
        }
    };
}


inline UUID generateUUID()
{
    static thread_local std::mt19937_64 gen(std::random_device{}());
    static thread_local std::uniform_int_distribution<uint64_t> dist;

    UUID id;
    uint64_t hi = dist(gen);
    uint64_t lo = dist(gen);
    std::memcpy(id.bytes.data(), &hi, 8);
    std::memcpy(id.bytes.data() + 8, &lo, 8);

    id.bytes[6] = (id.bytes[6] & 0x0F) | 0x40;
    id.bytes[8] = (id.bytes[8] & 0x3F) | 0x80;

    return id;
}

#include <optional>
#include <string_view>
#include <cctype>

inline std::optional<UUID> uuidFromString(std::string_view str)
{
    if (str.size() != 36)
        return std::nullopt;

    UUID uuid{};

    auto hexValue = [](char c) -> int
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    const int dashPositions[] = {8, 13, 18, 23};

    for (int pos : dashPositions)
    {
        if (str[pos] != '-')
            return std::nullopt;
    }

    size_t byteIndex = 0;

    for (size_t i = 0; i < str.size();)
    {
        if (str[i] == '-')
        {
            ++i;
            continue;
        }

        int hi = hexValue(str[i]);
        int lo = hexValue(str[i + 1]);

        if (hi < 0 || lo < 0)
            return std::nullopt;

        uuid.bytes[byteIndex++] = static_cast<uint8_t>((hi << 4) | lo);
        i += 2;
    }

    return uuid;
}

#endif // UUID_HPP