#ifndef STRUCTURE_TEMPLATE_HPP
#define STRUCTURE_TEMPLATE_HPP

#include "BBT.hpp"
#include "Block.hpp"
#include "BlockEntity.hpp"
#include "Prefab.hpp"

#include <cstdint>
#include <vector>

// How a cell of a structure template interacts with the existing chunk block.
enum class StructurePlacementMode : uint8_t
{
    // Skip the cell entirely — used to carve non-rectangular shapes out of a
    // rectangular bounding box (e.g. a round tower inside a square template).
    Skip = 0,

    // Overwrite whatever is in the world, including stone/dirt. Use for walls,
    // floors, roofs — anything the structure MUST own.
    Overwrite = 1,

    // Place only if the target is Air. Use for decorative overhangs that should
    // not dig into the surrounding terrain.
    AirOnly = 2,
};

struct StructureCell
{
    Block block;
    StructurePlacementMode mode;
};

// Hint for a block entity that should be created at (rx, ry) after the chunk
// finishes generating. The generator doesn't instantiate BlockEntity objects
// directly — StructureRegistry::stamp hands this to Chunk::setBlockEntity via
// BlockEntity::create(id, position).
struct StructureBlockEntity
{
    int rx;
    int ry;
    BlockEntityID id;
};

// Hint for an entity (mob, boss, dropped item, …) that should be spawned once
// the chunk is materialised on the server. generateChunk doesn't have a World&,
// so spawns go into Chunk::pending_spawns and are drained in
// ServerWorld::loadOrCreateChunk.
struct StructureSpawn
{
    int rx;
    int ry;
    PrefabID prefab;
};

// 2D side-view voxel template. Blockbit worlds have gravity, so only a
// horizontal mirror (flip X) makes sense — no 90/180/270.
enum class StructureRotation : uint8_t
{
    Original = 0,
    MirrorX  = 1,
};

struct StructureTemplate
{
    int width  = 0;
    int height = 0;

    // Point inside the bbox that aligns with the surface column at placement
    // time. `(anchor_x, anchor_y)` in template-local coords is the block whose
    // world position equals `(placement.origin_x, placement.anchor_y)`.
    int anchor_x = 0;
    int anchor_y = 0;

    // Row-major, size width*height. Cells are indexed as [y*width + x].
    std::vector<StructureCell> cells;

    std::vector<StructureBlockEntity> block_entities;
    std::vector<StructureSpawn>        spawns;

    const StructureCell& cellAt(int tx, int ty) const
    {
        return cells[static_cast<std::size_t>(ty) * static_cast<std::size_t>(width) + static_cast<std::size_t>(tx)];
    }

    // Transform a template-local (tx, ty) to world (wx, wy) given the placement
    // origin, anchor, and rotation. Keeps the math in ONE place — generator
    // code should never touch the fields directly.
    void toWorld(StructureRotation rot, int origin_x, int anchor_y,
                 int tx, int ty, int& wx, int& wy) const;

    // Inverse of toWorld: query which template cell corresponds to a world
    // position, returning false if the world position is outside the bbox.
    bool fromWorld(StructureRotation rot, int origin_x, int anchor_y,
                   int wx, int wy, int& tx, int& ty) const;

    Tag  serialize() const;
    void deserialize(const Tag& tag);
};

#endif // STRUCTURE_TEMPLATE_HPP
