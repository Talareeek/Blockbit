#ifndef PACKET_HPP
#define PACKET_HPP

#include <vector>
#include <cstdint>

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
    Chunk chunks[World::SIMULATION_DISTANCE + 1];
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

#endif // PACKET_HPP