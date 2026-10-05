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
    if(chunks.contains(chunk_position))
    {
        return chunks.at(chunk_position);
    }
    else
    {
        Chunk chunk{};
        chunk.chunk_position = chunk_position;
        chunk.dirty = true;
        chunk.meshDirty = true;
        chunk.generated = false;

        chunks[chunk_position] = chunk;

        return chunk;
    }
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

    if (trackBlockChanges)
    {
        pendingBlockUpdates.emplace_back(wx, wy, block);
    }
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

void World::tick(float dt)
{
    fluidTimer += dt;
    if(fluidTimer >= FLUID_TICK)
    {
        fluidTimer -= FLUID_TICK;
        updateFluids(*this);
    }
}

void updateFluids(World& world)
{
    /*
    int chunk = world.getEntities()[0].getComponent<TransformComponent>().position.x / (CHUNK_WIDTH);

    struct Vec2iHash
    {
        std::size_t operator()(const sf::Vector2i& v) const noexcept
        {
            return std::hash<long long>()((static_cast<long long>(v.x) << 32) ^ static_cast<unsigned int>(v.y));
        }
    };
    std::unordered_map<sf::Vector2i, Block, Vec2iHash> pending_changes;

    auto pendingLevel = [&](int wx, int wy) -> int
    {
        auto it = pending_changes.find({wx, wy});
        if (it == pending_changes.end() || it->second.id != BlockID::Water) return -1;
        return static_cast<int>(it->second.metadata);
    };

    for (int i = chunk - World::SIMULATION_DISTANCE / 2; i <= chunk + World::SIMULATION_DISTANCE / 2; ++i)
    {
        if (world.getChunk(i).generated == false) continue;

        for (int y = 0; y < CHUNK_HEIGHT; ++y)
        {
            for (int x = 0; x < CHUNK_WIDTH; ++x)
            {
                Block block = world.getChunk(i).blocks[y][x];
                if (block.id != BlockID::Water) continue;

                int worldX = i * CHUNK_WIDTH + x;
                int worldY = y;

                // DECAY — non-source water disappears if nothing feeds it
                if (block.metadata != static_cast<uint8_t>(WaterLevel::SOURCE))
                {
                    bool fed = false;
                    if (world.getBlock(worldX, worldY + 1).id == BlockID::Water) fed = true;
                    else
                    {
                        Block left = world.getBlock(worldX - 1, worldY);
                        Block right = world.getBlock(worldX + 1, worldY);
                        if (left.id == BlockID::Water && left.metadata > block.metadata) fed = true;
                        else if (right.id == BlockID::Water && right.metadata > block.metadata) fed = true;
                    }
                    if (!fed)
                    {
                        pending_changes[{worldX, worldY}] = {BlockID::Air, 0};
                        continue;
                    }
                }

                Block below = world.getBlock(worldX, worldY - 1);

                // TRY TO FLOW DOWNWARDS — only into Air, never overwrite Water (preserves SOURCE/FULL)
                if (below.id == BlockID::Air)
                {
                    if (pendingLevel(worldX, worldY - 1) < static_cast<int>(WaterLevel::FULL))
                    {
                        pending_changes[{worldX, worldY - 1}] = {BlockID::Water, static_cast<uint8_t>(WaterLevel::FULL)};
                    }
                    continue;
                }

                // If water is directly below, no sideways spread (water already has somewhere to go / is settled)
                if (below.id == BlockID::Water) continue;

                // TRY TO FLOW SIDEWAYS — never into solid blocks
                if (block.metadata > 1)
                {
                    uint8_t newLevel = static_cast<uint8_t>((block.metadata < 9) ? block.metadata - 1 : 7);

                    auto tryFlow = [&](int tx, int ty)
                    {
                        Block target = world.getBlock(tx, ty);
                        if (target.id != BlockID::Air && target.id != BlockID::Water) return;
                        if (target.id == BlockID::Water && target.metadata >= newLevel) return;
                        if (pendingLevel(tx, ty) >= static_cast<int>(newLevel)) return;

                        pending_changes[{tx, ty}] = {BlockID::Water, newLevel};
                    };

                    tryFlow(worldX - 1, worldY);
                    tryFlow(worldX + 1, worldY);
                }
            }
        }
    }

    for(auto& a : pending_changes)
    {
        world.setBlock(a.first.x, a.first.y, a.second);
    }
    */
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

    int entity_chunk = transform.position.x / CHUNK_WIDTH;

    return
    {
        static_cast<float>((entity_chunk - SIMULATION_DISTANCE) * CHUNK_WIDTH),
        static_cast<float>((entity_chunk + SIMULATION_DISTANCE) * CHUNK_WIDTH + CHUNK_WIDTH)
    };
}