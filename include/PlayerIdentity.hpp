#ifndef PLAYER_IDENTITY_HPP
#define PLAYER_IDENTITY_HPP

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>

// A player's stable identity. Steam-ready: once we're linked against
// Steamworks the transport fills `id` with the connecting peer's CSteamID.
// Until then `id` is deterministically derived from `display_name` so saves,
// bans and operator lists already key on an opaque 64-bit handle.
struct PlayerIdentity
{
    uint64_t    id = 0;
    std::string display_name;

    PlayerIdentity() = default;
    PlayerIdentity(uint64_t id, std::string display_name)
        : id(id), display_name(std::move(display_name)) {}

    bool valid() const { return id != 0; }

    // Two identities refer to the same player iff their ids match. The
    // display name is a hint (Steam personas rename themselves, non-Steam
    // nicks are typed fresh per launch) and must not be part of equality.
    friend bool operator==(const PlayerIdentity& a, const PlayerIdentity& b) { return a.id == b.id; }
    friend bool operator!=(const PlayerIdentity& a, const PlayerIdentity& b) { return a.id != b.id; }
};

// FNV-1a 64-bit over the display name. Deterministic across runs and
// platforms — the whole non-Steam identity story rests on this being stable.
inline uint64_t hashDisplayName(std::string_view name)
{
    constexpr uint64_t FNV_OFFSET = 0xcbf29ce484222325ULL;
    constexpr uint64_t FNV_PRIME  = 0x100000001b3ULL;

    uint64_t h = FNV_OFFSET;
    for (unsigned char c : name)
    {
        h ^= static_cast<uint64_t>(c);
        h *= FNV_PRIME;
    }
    // Reserve 0 for "unassigned" so callers can treat id==0 as "no identity".
    return h == 0 ? FNV_PRIME : h;
}

// Build an identity for a nickname-only build (no Steam). The id is a stable
// hash of the display name so replaying the same nick yields the same saves
// and ban records.
inline PlayerIdentity makeIdentityFromNickname(std::string nickname)
{
    uint64_t id = hashDisplayName(nickname);
    return PlayerIdentity(id, std::move(nickname));
}

// 16-char lowercase hex — safe as a filesystem path component, unlike raw
// display names which could contain slashes, spaces or path traversal.
inline std::string identityIdToHex(uint64_t id)
{
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016lx", static_cast<unsigned long>(id));
    return std::string(buf, 16);
}

namespace std
{
    template<> struct hash<PlayerIdentity>
    {
        size_t operator()(const PlayerIdentity& p) const noexcept
        {
            return std::hash<uint64_t>{}(p.id);
        }
    };
}

#endif // PLAYER_IDENTITY_HPP
