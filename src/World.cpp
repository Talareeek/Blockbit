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
#include "../include/WorldLightView.hpp"

#include <zstd.h>
#include <iostream>
#include <cmath>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <any>

Tag GenerationProperties::serialize() const
{
    TagCompound compound;

    compound["type"] = Tag(static_cast<uint32_t>(type));
    compound["spawn_item_chest_column"] = Tag(spawn_item_chest_column);
    compound["noodle_threshold"] = Tag(noodle_threshold);

    NormalWorldGeneratorProperties normal{base_height, height_scale, frequency, amplitude, persistence};
    compound["normal_properties"] = normal.serialize();

    FlatWorldProperties flat;
    flat.layers = flat_layers;
    compound["flat_properties"] = flat.serialize();

    return Tag(compound);
}

void GenerationProperties::deserialize(const Tag& tag)
{
    const TagCompound& compound = tag.get<TagCompound>();

    auto type_iter = compound.find("type");
    if (type_iter != compound.end())
    {
        type = static_cast<GenerationType>(type_iter->second.get<uint32_t>());
    }
    else
    {
        auto flat_iter = compound.find("flat");
        type = (flat_iter != compound.end() && flat_iter->second.get<bool>())
             ? GenerationType::Flat : GenerationType::Normal;
    }

    auto chest_iter = compound.find("spawn_item_chest_column");
    if (chest_iter != compound.end()) spawn_item_chest_column = chest_iter->second.get<bool>();

    auto noodle_iter = compound.find("noodle_threshold");
    if (noodle_iter != compound.end()) noodle_threshold = noodle_iter->second.get<float>();

    auto normal_iter = compound.find("normal_properties");
    if (normal_iter != compound.end())
    {
        NormalWorldGeneratorProperties normal{};
        normal.deserialize(normal_iter->second);
        base_height  = normal.base_height;
        height_scale = normal.height_scale;
        frequency    = normal.frequency;
        amplitude    = normal.amplitude;
        persistence  = normal.persistence;
    }
    else
    {
        base_height  = compound.at("base_height").get<float>();
        height_scale = compound.at("height_scale").get<float>();
        frequency    = compound.at("frequency").get<float>();
        amplitude    = compound.at("amplitude").get<float>();
        persistence  = compound.at("persistence").get<float>();
    }

    auto flat_props_iter = compound.find("flat_properties");
    if (flat_props_iter != compound.end())
    {
        FlatWorldProperties flat;
        flat.deserialize(flat_props_iter->second);
        flat_layers = std::move(flat.layers);
    }
    else
    {
        flat_layers.clear();
        auto layers_iter = compound.find("flat_layers");
        if (layers_iter != compound.end())
        {
            const TagList& layers_list = layers_iter->second.get<TagList>();
            flat_layers.reserve(layers_list.size());
            for (const Tag& layer_tag : layers_list)
            {
                flat_layers.push_back(FlatWorldLayer::deserialize(layer_tag));
            }
        }
    }

    if (flat_layers.empty())
    {
        flat_layers = FlatWorldProperties::defaults().layers;
    }
}

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

    // Capture the previous block's lighting attributes before mutating. We
    // can't read them after the write and the relighter needs the delta.
    Block previous = chunk.blocks[local_y][local_x];
    uint8_t prev_emission = 0, prev_opacity = 15;
    if (auto it = blockDatabase.find(previous.id); it != blockDatabase.end())
    {
        prev_emission = it->second.light_emission;
        prev_opacity  = it->second.light_opacity;
    }
    uint8_t next_emission = 0, next_opacity = 15;
    if (auto it = blockDatabase.find(block.id); it != blockDatabase.end())
    {
        next_emission = it->second.light_emission;
        next_opacity  = it->second.light_opacity;
    }

    chunk.blocks[local_y][local_x] = block;
    chunk.dirty = true;
    chunk.meshDirty = true;
    chunk.generated = true;

    // Connected-texture blocks on the border sample into the adjacent chunk, so
    // a write at local_x 0 / CHUNK_WIDTH-1 has to rebuild that neighbor's mesh
    // too — otherwise the seam stays stale until the neighbor is touched.
    if (local_x == 0)
    {
        if (auto nb = chunks.find(chunk_position - 1); nb != chunks.end())
            nb->second.meshDirty = true;
    }
    else if (local_x == CHUNK_WIDTH - 1)
    {
        if (auto nb = chunks.find(chunk_position + 1); nb != chunks.end())
            nb->second.meshDirty = true;
    }

    // Lightmap must only be recomputed once the lightmap vectors have been
    // sized at least once by initializeChunkSkyLight; skipping the pass for
    // uninitialized chunks also avoids partial writes before the first lit
    // chunk sweep.
    if (chunk.light.initialized)
    {
        WorldLightView view(*this);
        relightOnBlockChange(view, wx, wy,
                             prev_emission, prev_opacity,
                             next_emission, next_opacity);
    }
}

void World::setBlock(sf::Vector2i position, Block block)
{
    setBlock(position.x, position.y, block);
}

uint8_t World::getBlockLight(int wx, int wy) const
{
    if (wy < 0 || wy >= CHUNK_HEIGHT) return 0;
    int cp = (wx >= 0) ? wx / CHUNK_WIDTH : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;
    auto it = chunks.find(cp);
    if (it == chunks.end() || it->second.light.block_light.empty()) return 0;
    int local_x = wx - cp * CHUNK_WIDTH;
    return it->second.light.block_light[static_cast<size_t>(wy) * CHUNK_WIDTH + local_x];
}

uint8_t World::getSkyLight(int wx, int wy) const
{
    if (wy < 0 || wy >= CHUNK_HEIGHT) return 0;
    int cp = (wx >= 0) ? wx / CHUNK_WIDTH : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;
    auto it = chunks.find(cp);
    if (it == chunks.end() || it->second.light.sky_light.empty()) return 0;
    int local_x = wx - cp * CHUNK_WIDTH;
    return it->second.light.sky_light[static_cast<size_t>(wy) * CHUNK_WIDTH + local_x];
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

