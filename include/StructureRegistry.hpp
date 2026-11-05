#ifndef STRUCTURE_REGISTRY_HPP
#define STRUCTURE_REGISTRY_HPP

#include "StructureTemplate.hpp"
#include "Chunk.hpp"
#include "Climate.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

// Numeric structure identifier. Start the list empty — users of this system
// extend the enum and register templates in StructureRegistry::init().
// Keep IDs stable once released, they're hashed into the deterministic
// placement RNG.
enum class StructureID : uint32_t
{
    None = 0,
    // ... user-defined structures go here (e.g. Village_House, Dungeon_Small, …)
};

// Placement rule set attached to a template. Controls where the structure may
// appear during world generation. Everything a biome- or height-based filter
// needs lives here — the generator does not look at the template itself.
struct StructureRule
{
    StructureID id = StructureID::None;

    // Deterministic grid cell size (in world blocks). The placement scan hashes
    // (grid_x, StructureID) to pick exactly one candidate world_x per cell.
    // Larger grid => sparser placement. Must be >= template width + 2.
    int grid = 128;

    // Rejection roll in [0, 1]. 0 disables, 1 forces every candidate.
    float spawn_chance = 0.0f;

    // Inclusive altitude window applied to the surface column at the structure
    // origin. World outside this range skips the roll. Use e.g. (SEA_LEVEL+3,
    // 200) for above-sea structures, (10, 60) for buried dungeons.
    int min_surface_y = 0;
    int max_surface_y = 255;

    // How the structure anchors vertically.
    enum class Anchor : uint8_t
    {
        // placement.anchor_y = surface(origin_x) — structure sits on the
        // ground. Common for buildings/trees/shrines.
        Surface = 0,
        // placement.anchor_y = constant target_y — use together with
        // min/max_surface_y to e.g. bury a dungeon at fixed depth.
        Fixed   = 1,
    };
    Anchor anchor_mode = Anchor::Surface;
    int    target_y    = 0;

    // Biome allow-list. Empty vector = any biome.
    std::vector<Biome> biomes;

    // Whether the horizontal mirror is a valid variant. If true, the placement
    // scan rolls a 50/50 between Original and MirrorX.
    bool allow_mirror = true;
};

class World;
struct Chunk;

class StructureRegistry
{
public:

    static void add(StructureID id, StructureTemplate tmpl, StructureRule rule);

    static const StructureTemplate& getTemplate(StructureID id);
    static const StructureRule&     getRule(StructureID id);

    // Iterate every registered rule. Used by the generator placement scan.
    static const std::unordered_map<StructureID, StructureRule>& rules();

    // Returns the max template width across all registrations, so generators
    // can size their cross-chunk neighbour search. Returns 0 if empty.
    static int maxTemplateWidth();

    // Write the template's cells into `chunk` at the given placement. Cells
    // that fall outside the chunk's local x range are silently dropped — this
    // is intentional, mirroring NormalWorldGenerator::setFeatureBlock so a
    // structure straddling chunks is stamped correctly by each neighbour.
    //
    // Block entities and entity spawns are only emitted into the chunk when
    // the owning column lives in this chunk — that way a cross-chunk stamp
    // doesn't create duplicates.
    static void stamp(Chunk& chunk,
                      StructureID id,
                      int origin_x,
                      int anchor_y,
                      StructureRotation rotation);

    // Called once at startup, next to PrefabRegistry::init(). Empty by default
    // — this is where the user adds their structures.
    static void init();

private:

    static std::unordered_map<StructureID, StructureTemplate> templates;
    static std::unordered_map<StructureID, StructureRule>     rule_map;
};

#endif // STRUCTURE_REGISTRY_HPP
