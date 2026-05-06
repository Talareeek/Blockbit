#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <cstdint>
#include <vector>
#include <cstring>

namespace net
{

using EntityID = uint32_t;
using Tick     = uint32_t;

enum class PacketType : uint8_t
{
    JOIN,
    INPUT,

    FULL_STATE,
    SNAPSHOT,

    CHUNK_DATA,
    BLOCK_UPDATE,

    SPAWN,
    DESTROY
};

using Buffer = std::vector<char>;

template<typename T>
inline void write(Buffer& buf, const T& value)
{
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
    const char* ptr = reinterpret_cast<const char*>(&value);
    buf.insert(buf.end(), ptr, ptr + sizeof(T));
}

template<typename T>
inline T read(const char*& ptr)
{
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
    T value;
    std::memcpy(&value, ptr, sizeof(T));
    ptr += sizeof(T);
    return value;
}

struct InputPacket
{
    EntityID player_id;
    uint8_t  flags;

    enum : uint8_t
    {
        LEFT  = 1 << 0,
        RIGHT = 1 << 1,
        JUMP  = 1 << 2
    };
};

struct EntityState
{
    EntityID id;

    float x, y;
    float vx, vy;
};

struct SnapshotHeader
{
    Tick tick;
    uint32_t entity_count;
};

struct SpawnPacket
{
    EntityID id;
    uint8_t type;

    float x, y;
};

struct DestroyPacket
{
    EntityID id;
};

struct BlockUpdatePacket
{
    int32_t x, y;
    uint16_t block_id;
};

inline Buffer make_header(PacketType type)
{
    Buffer buf;
    write(buf, type);
    return buf;
}

inline Buffer build_snapshot(Tick tick, const std::vector<EntityState>& entities)
{
    Buffer buf = make_header(PacketType::SNAPSHOT);

    write(buf, tick);
    write(buf, static_cast<uint32_t>(entities.size()));

    for (const auto& e : entities)
    {
        write(buf, e.id);
        write(buf, e.x);
        write(buf, e.y);
        write(buf, e.vx);
        write(buf, e.vy);
    }

    return buf;
}

inline Buffer build_input(const InputPacket& input)
{
    Buffer buf = make_header(PacketType::INPUT);

    write(buf, input.player_id);
    write(buf, input.flags);

    return buf;
}

inline Buffer build_spawn(const SpawnPacket& p)
{
    Buffer buf = make_header(PacketType::SPAWN);

    write(buf, p.id);
    write(buf, p.type);
    write(buf, p.x);
    write(buf, p.y);

    return buf;
}

inline Buffer build_destroy(EntityID id)
{
    Buffer buf = make_header(PacketType::DESTROY);

    write(buf, id);

    return buf;
}

inline Buffer build_block_update(const BlockUpdatePacket& b)
{
    Buffer buf = make_header(PacketType::BLOCK_UPDATE);

    write(buf, b.x);
    write(buf, b.y);
    write(buf, b.block_id);

    return buf;
}

struct PacketView
{
    PacketType type;
    const char* ptr;
};

inline PacketView parse(const Buffer& buf)
{
    const char* ptr = buf.data();
    PacketType type = read<PacketType>(ptr);

    return { type, ptr };
}

} // namespace net

#endif // NETWORK_HPP