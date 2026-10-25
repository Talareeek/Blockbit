#ifndef CHUNK_HPP
#define CHUNK_HPP

#include "Block.hpp"
#include "BlockEntity.hpp"
#include "ChunkLightData.hpp"
#include "Climate.hpp"
#include "UUID.hpp"

#include <SFML/System/Vector2.hpp>
#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <unordered_set>

constexpr int CHUNK_WIDTH = 16;
constexpr int CHUNK_HEIGHT = 256;

struct Chunk
{
    int chunk_position;

    std::unordered_set<UUID> entity_ids;

    Block blocks[CHUNK_HEIGHT][CHUNK_WIDTH];

    Climate climates[CHUNK_WIDTH];
    Biome biomes[CHUNK_WIDTH];

    std::unordered_map<uint16_t, std::unique_ptr<BlockEntity>> block_entities;

    ChunkLightData light;

    bool dirty;
    bool generated;
    bool meshDirty;

    Chunk() = default;
    Chunk(Chunk&&) noexcept = default;
    Chunk& operator=(Chunk&&) noexcept = default;

    Chunk(const Chunk& other) { copyFrom(other); }
    Chunk& operator=(const Chunk& other)
    {
        if (this != &other) copyFrom(other);
        return *this;
    }

    static uint16_t packLocal(int local_x, int local_y)
    {
        return static_cast<uint16_t>(local_y * CHUNK_WIDTH + local_x);
    }

    BlockEntity* getBlockEntity(int local_x, int local_y)
    {
        auto it = block_entities.find(packLocal(local_x, local_y));
        if (it == block_entities.end()) return nullptr;
        return it->second.get();
    }

    const BlockEntity* getBlockEntity(int local_x, int local_y) const
    {
        auto it = block_entities.find(packLocal(local_x, local_y));
        if (it == block_entities.end()) return nullptr;
        return it->second.get();
    }

    void setBlockEntity(int local_x, int local_y, std::unique_ptr<BlockEntity> entity)
    {
        block_entities[packLocal(local_x, local_y)] = std::move(entity);
    }

    void removeBlockEntity(int local_x, int local_y)
    {
        block_entities.erase(packLocal(local_x, local_y));
    }

private:

    void copyFrom(const Chunk& other)
    {
        chunk_position = other.chunk_position;
        entity_ids = other.entity_ids;
        std::memcpy(blocks, other.blocks, sizeof(blocks));
        std::memcpy(climates, other.climates, sizeof(climates));
        std::memcpy(biomes, other.biomes, sizeof(biomes));
        light = other.light;
        dirty = other.dirty;
        generated = other.generated;
        meshDirty = other.meshDirty;

        block_entities.clear();
        for (const auto& [key, entity] : other.block_entities)
        {
            if (entity) block_entities.emplace(key, entity->clone());
        }
    }
};

#endif // CHUNK_HPP
