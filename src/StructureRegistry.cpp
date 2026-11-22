#include "../include/StructureRegistry.hpp"
#include "../include/ChestBlockEntity.hpp"
#include "../include/Chunk.hpp"

std::unordered_map<StructureKey, StructureTemplate>            StructureRegistry::templates;
std::unordered_map<StructureKey, StructureRule>                StructureRegistry::rule_map;
std::unordered_map<std::string, LootTable>                    StructureRegistry::loot_tables;
std::unordered_map<std::string, std::vector<JigsawPoolEntry>> StructureRegistry::jigsaw_pools;

namespace
{
    inline uint32_t mix32(uint32_t h)
    {
        h ^= h >> 16;
        h *= 0x85EBCA6Bu;
        h ^= h >> 13;
        h *= 0xC2B2AE35u;
        h ^= h >> 16;
        return h;
    }

    inline uint32_t advance(uint32_t& state)
    {
        state = mix32(state + 0x9E3779B9u);
        return state;
    }
}

void StructureRegistry::add(const StructureKey& id, StructureTemplate tmpl, StructureRule rule)
{
    rule.id = id;
    templates[id] = std::move(tmpl);
    rule_map[id]  = rule;
}

const StructureTemplate& StructureRegistry::getTemplate(const StructureKey& id)
{
    return templates.at(id);
}

const StructureRule& StructureRegistry::getRule(const StructureKey& id)
{
    return rule_map.at(id);
}

const std::unordered_map<StructureKey, StructureRule>& StructureRegistry::rules()
{
    return rule_map;
}

int StructureRegistry::maxTemplateWidth()
{
    int m = 0;
    for (const auto& [id, tmpl] : templates)
        if (tmpl.width > m) m = tmpl.width;
    return m;
}

int StructureRegistry::maxTemplateHeight()
{
    int m = 0;
    for (const auto& [id, tmpl] : templates)
        if (tmpl.height > m) m = tmpl.height;
    return m;
}

int StructureRegistry::maxChainReach()
{
    const int side = std::max(maxTemplateWidth(), maxTemplateHeight());
    int max_depth = 0;
    for (const auto& [id, rule] : rule_map)
        if (rule.max_chain_depth > max_depth) max_depth = rule.max_chain_depth;
    return side * max_depth;
}

void StructureRegistry::stamp(Chunk& chunk,
                              const StructureKey& id,
                              int origin_x,
                              int anchor_y,
                              StructureRotation rotation,
                              uint32_t placement_seed)
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

            // Per-cell RNG seeded off placement + template-local coord. Keeps
            // cross-chunk stamps of the same structure deterministic AND
            // identical — the same (tx, ty) picks the same palette block and
            // probability roll regardless of which chunk is being generated.
            uint32_t rng = placement_seed;
            rng = mix32(rng ^ (static_cast<uint32_t>(tx) * 374761393u));
            rng = mix32(rng ^ (static_cast<uint32_t>(ty) * 668265263u));

            if (cell.probability < 255)
            {
                const uint32_t roll = advance(rng) & 0xFFu;
                if (roll >= cell.probability) continue;
            }

            Block to_place = cell.block;
            if (cell.palette >= 0)
                to_place = tmpl.pickFromPalette(cell.palette, rng);

            if (cell.mode == StructurePlacementMode::AirOnly &&
                chunk.blocks[wy][local_x].id != Blocks::Air)
                continue;

            if (chunk.blocks[wy][local_x].id == Blocks::Bedrock) continue;

            chunk.blocks[wy][local_x] = to_place;
        }
    }

    for (const StructureBlockEntity& be : tmpl.block_entities)
    {
        int wx = 0;
        int wy = 0;
        tmpl.toWorld(rotation, origin_x, anchor_y, be.rx, be.ry, wx, wy);

        const int local_x = wx - chunk_min_x;
        if (local_x < 0 || local_x >= CHUNK_WIDTH) continue;
        if (wy < 0 || wy >= CHUNK_HEIGHT) continue;

        auto block_entity = BlockEntity::create(be.id, sf::Vector2i{wx, wy});
        if (!block_entity) continue;

        if (!be.loot_table.empty() && be.id == BlockEntityID::Chest)
        {
            if (auto* chest = dynamic_cast<ChestBlockEntity*>(block_entity.get()))
            {
                // Loot seed mixes the placement seed with world coordinates so
                // two chests inside the same structure get different rolls, but
                // the same chest regenerated from another chunk produces the
                // same contents (idempotent seeding).
                uint32_t loot_seed = placement_seed;
                loot_seed = mix32(loot_seed ^ static_cast<uint32_t>(wx) * 2654435761u);
                loot_seed = mix32(loot_seed ^ static_cast<uint32_t>(wy) * 40503u);
                applyLootToChest(*chest, be.loot_table, loot_seed);
            }
        }

        chunk.setBlockEntity(local_x, wy, std::move(block_entity));
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

void StructureRegistry::addLootTable(const std::string& key, LootTable table)
{
    loot_tables[key] = std::move(table);
}

const LootTable* StructureRegistry::findLootTable(const std::string& key)
{
    auto it = loot_tables.find(key);
    return it == loot_tables.end() ? nullptr : &it->second;
}

void StructureRegistry::applyLootToChest(ChestBlockEntity& chest,
                                         const std::string& loot_key,
                                         uint32_t seed)
{
    const LootTable* table = findLootTable(loot_key);
    if (!table || table->entries.empty()) return;

    // Keep author-placed loot (if any) — only roll when the chest is empty.
    for (const auto& s : chest.stacks)
        if (s.itemID != Items::None && s.quantity > 0) return;

    uint32_t total_weight = 0;
    for (const auto& e : table->entries) total_weight += e.weight;
    if (total_weight == 0) return;

    uint32_t rng = seed;
    const uint16_t min_r = table->min_rolls;
    const uint16_t max_r = std::max(min_r, table->max_rolls);
    const uint16_t span  = static_cast<uint16_t>(max_r - min_r + 1);
    const uint16_t rolls = static_cast<uint16_t>(min_r + (advance(rng) % span));

    for (uint16_t r = 0; r < rolls; r++)
    {
        const uint32_t roll = advance(rng) % total_weight;
        uint32_t acc = 0;
        const LootTableEntry* picked = nullptr;
        for (const auto& e : table->entries)
        {
            acc += e.weight;
            if (roll < acc) { picked = &e; break; }
        }
        if (!picked) picked = &table->entries.back();

        const uint16_t cspan = static_cast<uint16_t>(
            std::max<uint16_t>(picked->max_count, picked->min_count) - picked->min_count + 1);
        const uint16_t count = static_cast<uint16_t>(picked->min_count + (advance(rng) % cspan));
        if (count == 0 || picked->item == Items::None) continue;

        // Drop into the first empty slot — avoids stacking across multiple
        // rolls so chests look varied rather than one huge pile.
        for (auto& s : chest.stacks)
        {
            if (s.itemID == Items::None || s.quantity == 0)
            {
                s = ItemStack{picked->item, count, std::nullopt};
                break;
            }
        }
    }
}

void StructureRegistry::addJigsawPool(const std::string& key, std::vector<JigsawPoolEntry> entries)
{
    jigsaw_pools[key] = std::move(entries);
}

const std::vector<JigsawPoolEntry>* StructureRegistry::findJigsawPool(const std::string& key)
{
    auto it = jigsaw_pools.find(key);
    return it == jigsaw_pools.end() ? nullptr : &it->second;
}

void StructureRegistry::init()
{
    // Content comes from pack JSON — nothing to seed here.
}
