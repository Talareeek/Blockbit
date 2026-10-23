#include "../include/World.hpp"
#include "../include/Block.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/GameCommon.hpp"
#include "../include/Entity.hpp"
#include "../include/PreserveComponent.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/ExplosiveComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/AIComponent.hpp"
#include "../include/PlayerControlledComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/BBT.hpp"
#include "../include/NormalWorldGenerator.hpp"

#include <zstd.h>
#include <iostream>
#include <cmath>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <any>

Chunk& World::getChunk(int chunk_position)
{
    auto it = chunks.find(chunk_position);
    if (it != chunks.end()) return it->second;

    Chunk& chunk = chunks[chunk_position];
    chunk.chunk_position = chunk_position;
    chunk.dirty = true;
    chunk.meshDirty = true;
    chunk.generated = false;

    return chunk;
}


Block World::getBlock(int world_x, int world_y)
{
    if(world_y < 0 || world_y >= CHUNK_HEIGHT) return {BlockID::Air, 0};

    int chunk_position = (world_x >= 0) ? world_x / CHUNK_WIDTH : (world_x - CHUNK_WIDTH + 1) / CHUNK_WIDTH;

    if(!chunks.contains(chunk_position)) return {BlockID::Air, 0};

    int local_x = world_x - chunk_position * CHUNK_WIDTH;
    int local_y = world_y;

    return chunks[chunk_position].blocks[local_y][local_x];
}

Climate World::climateAt(int wx)
{
    int chunk_position = (wx >= 0) ? wx / CHUNK_WIDTH : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;
    int local_x = wx - chunk_position * CHUNK_WIDTH;

    return chunks[chunk_position].climates[local_x];
}

Biome World::biomeAt(int wx)
{
    int chunk_position = (wx >= 0) ? wx / CHUNK_WIDTH : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;
    int local_x = wx - chunk_position * CHUNK_WIDTH;

    return chunks[chunk_position].biomes[local_x];
}

void World::setBlock(int wx, int wy, Block block)
{
    if(wy < 0 || wy >= CHUNK_HEIGHT) return;

    int chunk_position = (wx >= 0)
    ? wx / CHUNK_WIDTH
    : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;

    int local_x = wx - chunk_position * CHUNK_WIDTH;
    int local_y = wy;

    // Only set block if chunk exists
    if(!chunks.contains(chunk_position)) return;

    Chunk& chunk = chunks[chunk_position];
    chunk.blocks[local_y][local_x] = block;
    chunk.dirty = true;
    chunk.meshDirty = true;
    chunk.generated = true;
}

void World::setBlock(sf::Vector2i position, Block block)
{
    setBlock(position.x, position.y, block);
}

BlockEntity* World::getBlockEntity(int world_x, int world_y)
{
    if(world_y < 0 || world_y >= CHUNK_HEIGHT) return nullptr;

    int chunk_position = (world_x >= 0) ? world_x / CHUNK_WIDTH : (world_x - CHUNK_WIDTH + 1) / CHUNK_WIDTH;

    auto it = chunks.find(chunk_position);
    if(it == chunks.end()) return nullptr;

    int local_x = world_x - chunk_position * CHUNK_WIDTH;
    return it->second.getBlockEntity(local_x, world_y);
}

void World::setBlockEntity(int world_x, int world_y, std::unique_ptr<BlockEntity> entity)
{
    if(world_y < 0 || world_y >= CHUNK_HEIGHT) return;

    int chunk_position = (world_x >= 0) ? world_x / CHUNK_WIDTH : (world_x - CHUNK_WIDTH + 1) / CHUNK_WIDTH;

    auto it = chunks.find(chunk_position);
    if(it == chunks.end()) return;

    int local_x = world_x - chunk_position * CHUNK_WIDTH;
    it->second.setBlockEntity(local_x, world_y, std::move(entity));
    it->second.dirty = true;
}

void World::removeBlockEntity(int world_x, int world_y)
{
    if(world_y < 0 || world_y >= CHUNK_HEIGHT) return;

    int chunk_position = (world_x >= 0) ? world_x / CHUNK_WIDTH : (world_x - CHUNK_WIDTH + 1) / CHUNK_WIDTH;

    auto it = chunks.find(chunk_position);
    if(it == chunks.end()) return;

    int local_x = world_x - chunk_position * CHUNK_WIDTH;
    it->second.removeBlockEntity(local_x, world_y);
    it->second.dirty = true;
}

std::unordered_map<int, Chunk>& World::getChunks()
{
    return chunks;
}

std::unordered_map<UUID, Entity>& World::getEntities()
{
    return entities;
}

const std::unordered_map<UUID, Entity>& World::getEntities() const
{
    return entities;
}

void World::addEntity(Entity entity)
{
    UUID id = entity.getID();

    if(!entity.hasComponent<TransformComponent>())
    {
        Chunk& chunk = getChunk(0);
        chunk.entity_ids.insert(id);
        chunk.dirty = true;        
    }
    else
    {
        Chunk& chunk = getChunk(entity.getComponent<TransformComponent>().chunkPosition());
        entity.getComponent<TransformComponent>().previous_position = entity.getComponent<TransformComponent>().position;
        chunk.entity_ids.insert(id);
        chunk.dirty = true;  
    }    

    entities.emplace(id, std::move(entity));
}

void World::removeEntity(UUID id)
{
    auto it = entities.find(id);
    if (it == entities.end()) return;

    if (it->second.hasComponent<TransformComponent>())
    {
        auto& transform = it->second.getComponent<TransformComponent>();
        int chunk_position = transform.chunkPosition();

        auto chunkIt = chunks.find(chunk_position);
        if (chunkIt != chunks.end())
        {
            chunkIt->second.entity_ids.erase(id);
            chunkIt->second.dirty = true;
        }
    }

    entities.erase(it);
}

Entity& World::getEntity(UUID id)
{
    if(entities.contains(id)) return getEntities().at(id);

    throw std::runtime_error("No entity found");
}

bool World::doesEntityExist(UUID id) const
{
    return entities.contains(id);
}

void World::tick(float /*dt*/)
{
    // Water flow used to run on a global sweep here; it's now driven per-cell
    // by BlockUpdatePipeline (see water_* callbacks in Block.cpp).
}






std::vector<UUID> World::getPlayerEntityIDs() const
{
    std::vector<UUID> ids;
    for (const auto& [id, entity] : entities)
    {
        if (entity.hasComponent<PlayerControlledComponent>())
            ids.push_back(entity.getID());
    }
    return ids;
}

std::pair<double, double> World::getSimulationRangeForEntity(const UUID entity)
{
    auto& transform = getEntity(entity).getComponent<TransformComponent>();

    int entity_chunk = transform.chunkPosition();

    return
    {
        static_cast<float>((entity_chunk - SIMULATION_DISTANCE) * CHUNK_WIDTH),
        static_cast<float>((entity_chunk + SIMULATION_DISTANCE) * CHUNK_WIDTH + CHUNK_WIDTH)
    };
}

void World::refreshChunkStates()
{
    loaded_chunk_positions.clear();
    lazy_chunk_positions.clear();

    for (const auto& [id, entity] : entities)
    {
        if (!entity.hasComponent<PlayerControlledComponent>()) continue;
        if (!entity.hasComponent<TransformComponent>()) continue;

        int player_chunk = entity.getComponent<TransformComponent>().chunkPosition();

        for (int offset = -LAZY_DISTANCE; offset <= LAZY_DISTANCE; ++offset)
        {
            int chunk_position = player_chunk + offset;
            if (std::abs(offset) <= SIMULATION_DISTANCE)
            {
                loaded_chunk_positions.insert(chunk_position);
            }
            else
            {
                lazy_chunk_positions.insert(chunk_position);
            }
        }
    }

    // A chunk simultaneously inside one player's loaded radius and another's
    // lazy ring must be treated as loaded.
    for (int chunk_position : loaded_chunk_positions)
    {
        lazy_chunk_positions.erase(chunk_position);
    }
}

ChunkState World::chunkState(int chunk_position) const
{
    if (loaded_chunk_positions.contains(chunk_position)) return ChunkState::Loaded;
    if (lazy_chunk_positions.contains(chunk_position))   return ChunkState::Lazy;
    return ChunkState::Unloaded;
}

bool World::isChunkLoaded(int chunk_position) const
{
    return loaded_chunk_positions.contains(chunk_position);
}

bool World::isChunkLazy(int chunk_position) const
{
    return lazy_chunk_positions.contains(chunk_position);
}