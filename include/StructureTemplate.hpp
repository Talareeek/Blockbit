#ifndef STRUCTURE_TEMPLATE_HPP
#define STRUCTURE_TEMPLATE_HPP

#include "BBT.hpp"
#include "Block.hpp"
#include "BlockEntity.hpp"
#include "Prefab.hpp"

#include <cstdint>
#include <string>
#include <vector>

// How a cell of a structure template interacts with the existing chunk block.
enum class StructurePlacementMode : uint8_t
{
    Skip      = 0, // Carve non-rectangular shapes — cell is a no-op
    Overwrite = 1, // Always replace the target (walls, floors)
    AirOnly   = 2, // Only replace Air (decorative overhangs)
};

// Per-cell contents. `palette` is an index into StructureTemplate::palettes;
// when non-negative, the generator picks one block from that palette at stamp
// time using its placement-local RNG and ignores `block`. `probability` is a
// 0..255 chance the cell is stamped at all (255 = always, used for ruined
// variants).
struct StructureCell
{
    Block                  block;
    StructurePlacementMode mode;
    int16_t                palette     = -1;
    uint8_t                probability = 255;
};

// A weighted pool of blocks. Weights are uint16; sum must be > 0 for the
// palette to produce anything. Empty palette or zero sum => resolves to Air.
struct StructurePalette
{
    std::vector<Block>    blocks;
    std::vector<uint16_t> weights;
};

// Named loot table reference attached to a block entity marker. The actual
// table lives in LootTableRegistry (see StructureRegistry.hpp). Empty string
// means "no loot" — the chest spawns empty.
struct StructureBlockEntity
{
    int           rx;
    int           ry;
    BlockEntityID id;
    std::string   loot_table; // registry key, e.g. "blockbit:mineshaft_chest"
};

struct StructureSpawn
{
    int       rx;
    int       ry;
    PrefabID  prefab;
};

// Facing of a jigsaw connector. The generator joins two jigsaws when the
// target jigsaw faces OPPOSITE the source (Left ↔ Right, Up ↔ Down).
enum class StructureJigsawFacing : uint8_t
{
    Right = 0,
    Left  = 1,
    Up    = 2,
    Down  = 3,
};

// A connection point. Templates with jigsaws can be chained during world-gen:
// the generator starts from a "seed" placement (one that comes out of the
// grid-cell rule scan) and extends by picking a template from `pool` whose
// own jigsaw has the opposite facing and compatible `connector`. The jigsaw
// block itself is removed from the stamp (overwritten with Air) when the
// chain actually extends through it.
struct StructureJigsaw
{
    int                    rx;
    int                    ry;
    StructureJigsawFacing  facing;
    std::string            connector;   // compatibility key, e.g. "corridor_h"
    std::string            pool;        // registry key of the pool to draw from
};

enum class StructureRotation : uint8_t
{
    Original = 0,
    MirrorX  = 1,
    MirrorY  = 2,
    MirrorXY = 3, // both axes — Y-flip composed with X-flip
};

struct StructureTemplate
{
    int width  = 0;
    int height = 0;

    int anchor_x = 0;
    int anchor_y = 0;

    std::vector<StructureCell>        cells;
    std::vector<StructurePalette>     palettes;
    std::vector<StructureBlockEntity> block_entities;
    std::vector<StructureSpawn>       spawns;
    std::vector<StructureJigsaw>      jigsaws;

    // When set, the template may also be stamped with MirrorY (rolled 50/50
    // with the Original/MirrorX pair). Only relevant for templates that still
    // look right upside-down (dungeons with no floor/ceiling asymmetry, etc).
    bool allow_mirror_y = false;

    const StructureCell& cellAt(int tx, int ty) const
    {
        return cells[static_cast<std::size_t>(ty) * static_cast<std::size_t>(width) + static_cast<std::size_t>(tx)];
    }

    void toWorld(StructureRotation rot, int origin_x, int placement_anchor_y,
                 int tx, int ty, int& wx, int& wy) const;

    bool fromWorld(StructureRotation rot, int origin_x, int placement_anchor_y,
                   int wx, int wy, int& tx, int& ty) const;

    // Pick a block from the referenced palette using the given rng state.
    // Returns {Air, 0} when the palette is empty or has zero total weight.
    Block pickFromPalette(int palette_index, uint32_t& rng_state) const;

    // Transform a jigsaw's facing under the given rotation. Centralised so
    // callers never have to repeat the per-axis flip logic.
    static StructureJigsawFacing rotateFacing(StructureJigsawFacing f, StructureRotation rot);

    Tag  serialize() const;
    void deserialize(const Tag& tag);
};

#endif // STRUCTURE_TEMPLATE_HPP
