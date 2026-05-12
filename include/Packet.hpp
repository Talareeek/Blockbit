#ifndef PACKET_HPP
#define PACKET_HPP

#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include <stdexcept>
#include <type_traits>

#include "Chunk.hpp"
#include "World.hpp"

enum class PacketType : uint8_t
{
    Initialization,
    BlockUpdate,
    Snapshot,
    Spawn,
    Despawn,

    Input
};

struct InitializationPacket
{
    Chunk chunk;
};

struct BlockUpdatePacket
{
    int x;
    int y;
    Block block;
};

struct NetEntity
{
    uint32_t id;

    float x;
    float y;

    float size_x;
    float size_y;

    uint32_t textureID;

    int uv_x;
    int uv_y;
    int uv_size_x;
    int uv_size_y;

    uint32_t health;
    uint32_t maxHealth;

    struct InventorySlot
    {
        uint32_t itemID;
        uint32_t quantity;
    };

    std::vector<InventorySlot> inventory;
};

struct SnapshotPacket
{
    std::vector<NetEntity> entities;

    float dayTime;
    uint64_t days;
};

struct SpawnPacket
{
    uint32_t id;
};

struct DespawnPacket
{
    uint32_t id;
};

struct InputPacket
{
    uint32_t id;

    bool left;
    bool right;

    bool jump;
};


class PacketWriter
{
private:

    std::vector<char> data;

public:

    PacketWriter() = default;
    explicit PacketWriter(PacketType type);

    void writeBytes(const void* src, std::size_t n);

    template<typename T>
    void write(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "PacketWriter::write requires trivially copyable T");
        writeBytes(&value, sizeof(T));
    }

    void writeString(const std::string& s);

    const std::vector<char>& buffer() const { return data; }
    std::vector<char> release() { return std::move(data); }
};

class PacketReader
{
private:

    const char* ptr;
    const char* end;

public:

    PacketReader(const char* src, std::size_t n) : ptr(src), end(src + n) {}

    void readBytes(void* dst, std::size_t n);

    template<typename T>
    T read()
    {
        static_assert(std::is_trivially_copyable_v<T>, "PacketReader::read requires trivially copyable T");
        T value;
        readBytes(&value, sizeof(T));
        return value;
    }

    std::string readString();

    bool eof() const { return ptr >= end; }
    std::size_t remaining() const { return static_cast<std::size_t>(end - ptr); }
};

std::vector<char> serializePacket(const InitializationPacket& p);
std::vector<char> serializePacket(const BlockUpdatePacket& p);
std::vector<char> serializePacket(const SnapshotPacket& p);
std::vector<char> serializePacket(const SpawnPacket& p);
std::vector<char> serializePacket(const DespawnPacket& p);
std::vector<char> serializePacket(const InputPacket& p);

InitializationPacket deserializeInitialization(PacketReader& r);
BlockUpdatePacket    deserializeBlockUpdate(PacketReader& r);
SnapshotPacket       deserializeSnapshot(PacketReader& r);
SpawnPacket          deserializeSpawn(PacketReader& r);
DespawnPacket        deserializeDespawn(PacketReader& r);
InputPacket          deserializeInput(PacketReader& r);

#endif // PACKET_HPP
