#include "../include/StructureRegistry.hpp"
#include "../include/Chunk.hpp"

std::unordered_map<StructureID, StructureTemplate> StructureRegistry::templates;
std::unordered_map<StructureID, StructureRule>     StructureRegistry::rule_map;

void StructureRegistry::add(StructureID id, StructureTemplate tmpl, StructureRule rule)
{
    rule.id = id;
    templates[id] = std::move(tmpl);
    rule_map[id]  = rule;
}

const StructureTemplate& StructureRegistry::getTemplate(StructureID id)
{
    return templates.at(id);
}

const StructureRule& StructureRegistry::getRule(StructureID id)
{
    return rule_map.at(id);
}

const std::unordered_map<StructureID, StructureRule>& StructureRegistry::rules()
{
    return rule_map;
}

int StructureRegistry::maxTemplateWidth()
{
    int m = 0;
    for (const auto& [id, tmpl] : templates)
    {
        if (tmpl.width > m) m = tmpl.width;
    }
    return m;
}

void StructureRegistry::stamp(Chunk& chunk,
                              StructureID id,
                              int origin_x,
                              int anchor_y,
                              StructureRotation rotation)
{
    const auto t_it = templates.find(id);
    if (t_it == templates.end()) return;

    const StructureTemplate& tmpl = t_it->second;

    const int chunk_min_x = chunk.chunk_position * CHUNK_WIDTH;

    for (int ty = 0; ty < tmpl.height; ty++)
    {
        for (int tx = 0; tx < tmpl.width; tx++)
        {
            const StructureCell& cell = tmpl.cellAt(tx, ty);
            if (cell.mode == StructurePlacementMode::Skip) continue;

            int wx = 0;
            int wy = 0;
            tmpl.toWorld(rotation, origin_x, anchor_y, tx, ty, wx, wy);

            const int local_x = wx - chunk_min_x;
            if (local_x < 0 || local_x >= CHUNK_WIDTH) continue;
            if (wy < 0 || wy >= CHUNK_HEIGHT) continue;

            if (cell.mode == StructurePlacementMode::AirOnly &&
                chunk.blocks[wy][local_x].id != BlockID::Air)
                continue;

            chunk.blocks[wy][local_x] = cell.block;
        }
    }

    // Block entities and entity spawns only land in the chunk that OWNS the
    // relevant column — otherwise a structure stamped by both neighbours would
    // spawn each chest/entity twice.
    for (const StructureBlockEntity& be : tmpl.block_entities)
    {
        int wx = 0;
        int wy = 0;
        tmpl.toWorld(rotation, origin_x, anchor_y, be.rx, be.ry, wx, wy);

        const int local_x = wx - chunk_min_x;
        if (local_x < 0 || local_x >= CHUNK_WIDTH) continue;
        if (wy < 0 || wy >= CHUNK_HEIGHT) continue;

        auto block_entity = BlockEntity::create(be.id, sf::Vector2i{wx, wy});
        if (block_entity)
        {
            chunk.setBlockEntity(local_x, wy, std::move(block_entity));
        }
    }

    for (const StructureSpawn& sp : tmpl.spawns)
    {
        int wx = 0;
        int wy = 0;
        tmpl.toWorld(rotation, origin_x, anchor_y, sp.rx, sp.ry, wx, wy);

        const int local_x = wx - chunk_min_x;
        if (local_x < 0 || local_x >= CHUNK_WIDTH) continue;
        if (wy < 0 || wy >= CHUNK_HEIGHT) continue;

        PendingSpawn pending;
        pending.prefab   = sp.prefab;
        pending.position = {static_cast<double>(wx) + 0.5, static_cast<double>(wy)};
        chunk.pending_spawns.push_back(pending);
    }
}

void StructureRegistry::init()
{
    // User-defined structures register here. Editor saves land in
    // getStructuresPath() (~/Blockbit/structures on Linux). Typical load:
    //
    // #include "World.hpp"        // getStructuresPath
    // #include "BBT.hpp"
    // #include <fstream>
    //
    // std::ifstream in(getStructuresPath() / "village_house.bbt", std::ios::binary);
    // std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    //
    // BBT bbt = BBT::load(bytes);
    // StructureTemplate tmpl;
    // tmpl.deserialize(bbt.root().at("template"));
    //
    // StructureRule rule;
    // rule.grid          = 64;
    // rule.spawn_chance  = 0.25f;
    // rule.min_surface_y = 76;
    // rule.max_surface_y = 180;
    // rule.biomes        = {Biome::Plains, Biome::Forest};
    //
    // StructureRegistry::add(StructureID::Village_House, std::move(tmpl), rule);
}
