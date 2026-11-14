#ifndef STRUCTURE_REGISTRY_HPP
#define STRUCTURE_REGISTRY_HPP

#include "StructureTemplate.hpp"
#include "Chunk.hpp"
#include "Climate.hpp"
#include "Item.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

using StructureID = uint32_t;

StructureID structureIdFromKey(const std::string& key);

// Placement rule set attached to a template.
struct StructureRule
{
    StructureID id = 0;
    std::string key;

    int   grid         = 128;
    float spawn_chance = 0.0f;

    int min_surface_y = 0;
    int max_surface_y = 255;

    enum class Anchor : uint8_t
    {
        Surface      = 0,
        Fixed        = 1,
        BelowSurface = 2,
    };
    Anchor anchor_mode = Anchor::Surface;
    int    target_y    = 0;

    std::vector<Biome> biomes;

    bool allow_mirror = true;

    // Chain extension via jigsaws. 0 = no chaining, N = up to N pieces attached
    // beyond the root. The generator clips the chain at this depth even if
    // compatible pieces still exist in the pool.
    int max_chain_depth = 0;
};

// A single item line in a loot table. `min_count..max_count` inclusive rolled
// uniformly. `weight` is the relative chance against the pool's total.
struct LootTableEntry
{
    ItemID   item      = ItemID::None;
    uint16_t min_count = 1;
    uint16_t max_count = 1;
    uint16_t weight    = 1;
};

// A loot table rolls `rolls` times against the entries pool. Used to fill a
// freshly-generated chest when a structure block entity has a non-empty
// `loot_table` reference.
struct LootTable
{
    std::vector<LootTableEntry> entries;
    uint16_t                    min_rolls = 1;
    uint16_t                    max_rolls = 3;
};

// A weighted entry inside a jigsaw pool. Pools are looked up by name from
// each StructureJigsaw. The generator picks one member deterministically
// using the chain's RNG state.
struct JigsawPoolEntry
{
    StructureID id     = 0;
    uint16_t    weight = 1;
};

class World;
class ChestBlockEntity;
struct Chunk;

class StructureRegistry
{
public:

    static void add(StructureID id, StructureTemplate tmpl, StructureRule rule);

    static const StructureTemplate& getTemplate(StructureID id);
    static const StructureRule&     getRule(StructureID id);

    static const std::unordered_map<StructureID, StructureRule>& rules();

    static int maxTemplateWidth();
    static int maxTemplateHeight();
    static int maxChainReach(); // max_chain_depth * max_template_dim across all rules

    // Deterministic stamp. `placement_seed` is mixed into per-cell rolls
    // (palette picks, probability gates, loot) so every client/server
    // generating the same chunk reproduces the same stamp output.
    static void stamp(Chunk& chunk,
                      StructureID id,
                      int origin_x,
                      int anchor_y,
                      StructureRotation rotation,
                      uint32_t placement_seed);

    // ----- loot tables -----

    static void addLootTable(const std::string& key, LootTable table);
    static const LootTable* findLootTable(const std::string& key);

    // Fills a chest in-place from the named loot table using the given seed.
    // No-op if the key is unknown or the chest already holds items.
    static void applyLootToChest(ChestBlockEntity& chest,
                                 const std::string& loot_key,
                                 uint32_t seed);

    // ----- jigsaw pools -----

    static void addJigsawPool(const std::string& key, std::vector<JigsawPoolEntry> entries);
    static const std::vector<JigsawPoolEntry>* findJigsawPool(const std::string& key);

    static void init();

private:

    static std::unordered_map<StructureID, StructureTemplate>            templates;
    static std::unordered_map<StructureID, StructureRule>                rule_map;
    static std::unordered_map<std::string, LootTable>                    loot_tables;
    static std::unordered_map<std::string, std::vector<JigsawPoolEntry>> jigsaw_pools;
};

#endif // STRUCTURE_REGISTRY_HPP
