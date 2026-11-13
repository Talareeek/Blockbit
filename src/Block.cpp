#include "../include/Block.hpp"
#include "../include/BlockUpdatePipeline.hpp"
#include "../include/ServerWorld.hpp"
#include "../include/Prefab.hpp"

#include <algorithm>

// Vanilla block callbacks (named, not anonymous) so ContentProfiles can wire
// them into pack-loaded blocks by name. See include/ContentProfiles.hpp.
namespace blockbit_callbacks
{
    // Can a falling block pass through this cell on the way down? Air, water
    // and lava are passable; the fluid gets displaced by the final landing.
    bool falling_block_can_leave_into(Block b)
    {
        return b.id == BlockID::Air || b.id == BlockID::Water || b.id == BlockID::Lava;
    }

    bool is_falling_block(BlockID id)
    {
        return id == BlockID::Sand || id == BlockID::Gravel;
    }

    // Pulls `fall_delay_ticks` out of the block's GravityBlock behavior if
    // one is attached; otherwise falls back to the historical 2-tick delay.
    uint32_t falling_delay_for(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        const auto block = ctx.world.getBlock(pos.x, pos.y);
        if (auto it = blockDatabase.find(block.id); it != blockDatabase.end())
        {
            const auto& bh = it->second.behavior;
            if (bh.type == BlockBehaviorType::GravityBlock)
                if (const auto* p = std::get_if<GravityBlockParams>(&bh.params))
                    if (p->fall_delay_ticks > 0) return p->fall_delay_ticks;
        }
        return 2;
    }

    void falling_try_schedule_fall(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        Block below = ctx.world.getBlock(pos.x, pos.y - 1);
        if (!falling_block_can_leave_into(below)) return;
        ctx.pipeline.scheduleTick(pos, static_cast<int>(falling_delay_for(ctx, pos)));
    }

    void falling_on_place(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        falling_try_schedule_fall(ctx, pos);
    }

    void falling_on_post_placement(BlockUpdateContext& ctx, sf::Vector2i pos, uint8_t /*from_direction*/)
    {
        // A neighbour change may have opened a hole below us. Only reschedule
        // if we're still a falling block and we don't already have a pending tick.
        if (!is_falling_block(ctx.world.getBlock(pos.x, pos.y).id)) return;
        if (ctx.pipeline.hasScheduledTick(pos)) return;
        falling_try_schedule_fall(ctx, pos);
    }

    void falling_on_scheduled_tick(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        Block self = ctx.world.getBlock(pos.x, pos.y);
        if (!is_falling_block(self.id)) return;

        Block below = ctx.world.getBlock(pos.x, pos.y - 1);
        if (!falling_block_can_leave_into(below)) return;

        // Hand the block off to physics: clear the cell, spawn a FallingBlock
        // entity at the same world position. PhysicsSystem will drag it down,
        // FallingBlockSystem will lay it back down as a block on landing.
        // Position lines up with (pos.x, pos.y) because FallingBlock is 1x1
        // bottom-left-anchored, same as a tile.
        ctx.world.setBlock(pos.x, pos.y, {BlockID::Air, 0});
        spawnFallingBlock(
            ctx.world,
            {static_cast<double>(pos.x), static_cast<double>(pos.y)},
            self.id,
            self.metadata
        );
    }

    // Delay between a water cell being disturbed and its flow logic firing.
    // 30 server ticks @ 120 TPS = 0.25 s, which lets a vertical fall tick down
    // at a visible pace instead of teleporting to the floor in a single frame.
    // Used as the fallback when a fluid's CascadeFluid behavior doesn't set
    // tick_delay_ticks.
    constexpr int WATER_TICK_DELAY = 30;

    // Reads tick_delay_ticks from a CascadeFluid behavior attached to the
    // block at `pos`. Returns `fallback` for non-fluid blocks or when the
    // behavior leaves the field at zero.
    int cascade_delay_for(BlockUpdateContext& ctx, sf::Vector2i pos, int fallback)
    {
        const auto block = ctx.world.getBlock(pos.x, pos.y);
        if (auto it = blockDatabase.find(block.id); it != blockDatabase.end())
        {
            const auto& bh = it->second.behavior;
            if (bh.type == BlockBehaviorType::CascadeFluid)
                if (const auto* p = std::get_if<CascadeFluidParams>(&bh.params))
                    if (p->tick_delay_ticks > 0) return static_cast<int>(p->tick_delay_ticks);
        }
        return fallback;
    }

    bool water_cell_passable(Block b)
    {
        return b.id == BlockID::Air || b.id == BlockID::Water;
    }

    // A non-source cell survives only if something is actively supplying it:
    // water directly above (vertical column), or a stronger neighbour sideways.
    // Without that, it evaporates — same decay rule the old sweep used.
    bool water_is_fed(World& world, sf::Vector2i pos, uint8_t level)
    {
        if (level == WaterState::SOURCE) return true;

        if (world.getBlock(pos.x, pos.y + 1).id == BlockID::Water) return true;

        Block left  = world.getBlock(pos.x - 1, pos.y);
        Block right = world.getBlock(pos.x + 1, pos.y);
        if (left.id  == BlockID::Water && WaterState::unpack(left.metadata).level  > level) return true;
        if (right.id == BlockID::Water && WaterState::unpack(right.metadata).level > level) return true;
        return false;
    }

    void water_schedule_self(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        if (ctx.world.getBlock(pos.x, pos.y).id != BlockID::Water) return;
        if (ctx.pipeline.hasScheduledTick(pos)) return;
        ctx.pipeline.scheduleTick(pos, cascade_delay_for(ctx, pos, WATER_TICK_DELAY));
    }

    void water_on_place(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        water_schedule_self(ctx, pos);
    }

    void water_on_neighbor_changed(BlockUpdateContext& ctx, sf::Vector2i pos, sf::Vector2i /*neighbor*/)
    {
        water_schedule_self(ctx, pos);
    }

    void water_on_scheduled_tick(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        Block self = ctx.world.getBlock(pos.x, pos.y);
        if (self.id != BlockID::Water) return;

        const uint8_t level = WaterState::unpack(self.metadata).level;

        // Air-ing the cell fires NC on neighbours, which reschedules the next
        // weaker water in the chain — the decay cascade unwinds one tick-delay
        // at a time instead of in a single global sweep.
        if (!water_is_fed(ctx.world, pos, level))
        {
            ctx.world.setBlock(pos.x, pos.y, {BlockID::Air, 0});
            return;
        }

        Block below = ctx.world.getBlock(pos.x, pos.y - 1);

        // Free-fall into air always writes FULL regardless of our own level —
        // falling water keeps its strength until it hits something to spread on.
        if (below.id == BlockID::Air)
        {
            ctx.world.setBlock(pos.x, pos.y - 1, {BlockID::Water, WaterState{WaterState::FULL}.pack()});
            return;
        }

        // Water already underneath us: settled column, let that cell handle
        // whatever sideways propagation happens from the bottom.
        if (below.id == BlockID::Water) return;

        // Level 1 is the floor — no lower level to project sideways.
        if (level <= 1) return;

        // SOURCE and FULL both project level 7 sideways; everything else
        // decreases by one, matching the old sweep's reach.
        const uint8_t new_level = (level < WaterState::SOURCE)
            ? static_cast<uint8_t>(level - 1)
            : static_cast<uint8_t>(WaterState::FULL - 1);

        auto try_flow = [&](int tx, int ty)
        {
            Block target = ctx.world.getBlock(tx, ty);
            if (!water_cell_passable(target)) return;
            if (target.id == BlockID::Water && WaterState::unpack(target.metadata).level >= new_level) return;
            ctx.world.setBlock(tx, ty, {BlockID::Water, WaterState{new_level}.pack()});
        };

        try_flow(pos.x - 1, pos.y);
        try_flow(pos.x + 1, pos.y);
    }

    constexpr int CACTUS_MAX_HEIGHT = 3;

    void cactus_on_post_placement(BlockUpdateContext& ctx, sf::Vector2i pos, uint8_t /*from_direction*/)
    {
        Block self = ctx.world.getBlock(pos.x, pos.y);
        if (self.id != BlockID::Cactus) return;

        // Support (sand/cactus below) is handled by BlockData::support_blocks_below
        // via the pipeline — this callback only reconciles the is_top flag so
        // the mesh picks up the correct texture when the column changes.
        const bool should_be_top = ctx.world.getBlock(pos.x, pos.y + 1).id != BlockID::Cactus;
        const bool is_top = CactusState::unpack(self.metadata).is_top;
        if (should_be_top != is_top)
        {
            ctx.pipeline.setBlockSuppressNC(pos, {BlockID::Cactus, CactusState{should_be_top}.pack()});
        }
    }

    void cactus_on_random_tick(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        Block self = ctx.world.getBlock(pos.x, pos.y);
        if (self.id != BlockID::Cactus) return;

        // Growth is driven from the topmost cell so a 2-tall cactus only picks
        // up one random tick per sampling instead of one per segment.
        if (!CactusState::unpack(self.metadata).is_top) return;

        int height = 1;
        for (int y = pos.y - 1; y >= 0; --y)
        {
            if (ctx.world.getBlock(pos.x, y).id != BlockID::Cactus) break;
            ++height;
        }

        // Read max_height and grows_as from the block's PlantGrowth behavior
        // so modpack-defined plants with different caps / species go through
        // the same growth pipeline.
        int max_height = CACTUS_MAX_HEIGHT;
        BlockID grown  = BlockID::Cactus;
        if (auto it = blockDatabase.find(self.id); it != blockDatabase.end())
        {
            const auto& bh = it->second.behavior;
            if (bh.type == BlockBehaviorType::PlantGrowth)
                if (const auto* p = std::get_if<PlantGrowthParams>(&bh.params))
                {
                    if (p->max_height > 0) max_height = p->max_height;
                    if (p->grows_as != BlockID{}) grown = p->grows_as;
                }
        }

        if (height >= max_height) return;

        Block above = ctx.world.getBlock(pos.x, pos.y + 1);
        if (above.id != BlockID::Air) return;

        // Suppress NC so neighbouring sand/water don't fire logical updates for
        // what is logically a self-contained growth step.
        ctx.pipeline.setBlockSuppressNC(pos, {grown, CactusState{false}.pack()});
        ctx.pipeline.setBlockSuppressNC({pos.x, pos.y + 1}, {grown, CactusState{true}.pack()});
    }

    // Lava uses the same cascade as water but ticks three times slower and
    // caps horizontal reach at ~3 cells (half of water's ~7). Levels follow
    // LavaState (FULL=4, SOURCE=5) instead of WaterState.
    constexpr int LAVA_TICK_DELAY = 90;

    bool lava_cell_passable(Block b)
    {
        return b.id == BlockID::Air || b.id == BlockID::Lava;
    }

    bool lava_is_fed(World& world, sf::Vector2i pos, uint8_t level)
    {
        if (level == LavaState::SOURCE) return true;

        if (world.getBlock(pos.x, pos.y + 1).id == BlockID::Lava) return true;

        Block left  = world.getBlock(pos.x - 1, pos.y);
        Block right = world.getBlock(pos.x + 1, pos.y);
        if (left.id  == BlockID::Lava && LavaState::unpack(left.metadata).level  > level) return true;
        if (right.id == BlockID::Lava && LavaState::unpack(right.metadata).level > level) return true;
        return false;
    }

    void lava_schedule_self(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        if (ctx.world.getBlock(pos.x, pos.y).id != BlockID::Lava) return;
        if (ctx.pipeline.hasScheduledTick(pos)) return;
        ctx.pipeline.scheduleTick(pos, cascade_delay_for(ctx, pos, LAVA_TICK_DELAY));
    }

    void lava_on_place(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        lava_schedule_self(ctx, pos);
    }

    void lava_on_neighbor_changed(BlockUpdateContext& ctx, sf::Vector2i pos, sf::Vector2i /*neighbor*/)
    {
        lava_schedule_self(ctx, pos);
    }

    void lava_on_scheduled_tick(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        Block self = ctx.world.getBlock(pos.x, pos.y);
        if (self.id != BlockID::Lava) return;

        const uint8_t level = LavaState::unpack(self.metadata).level;

        if (!lava_is_fed(ctx.world, pos, level))
        {
            ctx.world.setBlock(pos.x, pos.y, {BlockID::Air, 0});
            return;
        }

        Block below = ctx.world.getBlock(pos.x, pos.y - 1);

        if (below.id == BlockID::Air)
        {
            ctx.world.setBlock(pos.x, pos.y - 1, {BlockID::Lava, LavaState{LavaState::FULL}.pack()});
            return;
        }

        if (below.id == BlockID::Lava) return;

        if (level <= 1) return;

        const uint8_t new_level = (level < LavaState::SOURCE)
            ? static_cast<uint8_t>(level - 1)
            : static_cast<uint8_t>(LavaState::FULL - 1);

        auto try_flow = [&](int tx, int ty)
        {
            Block target = ctx.world.getBlock(tx, ty);
            if (!lava_cell_passable(target)) return;
            if (target.id == BlockID::Lava && LavaState::unpack(target.metadata).level >= new_level) return;
            ctx.world.setBlock(tx, ty, {BlockID::Lava, LavaState{new_level}.pack()});
        };

        try_flow(pos.x - 1, pos.y);
        try_flow(pos.x + 1, pos.y);
    }
}

ItemID blockToItem(BlockID block)
{
    switch(block)
    {
        case BlockID::Stone: return ItemID::Stone;
        case BlockID::Grass: return ItemID::Grass;
        case BlockID::Dirt: return ItemID::Dirt;
        case BlockID::Cobblestone: return ItemID::Cobblestone;
        case BlockID::Obsidian: return ItemID::Obsidian;
        case BlockID::Bedrock: return ItemID::Bedrock;
        case BlockID::Coal_Ore: return ItemID::Coal_Ore;
        case BlockID::Iron_Ore: return ItemID::Iron_Ore;
        case BlockID::Gold_Ore: return ItemID::Gold_Ore;
        case BlockID::Diamond_Ore: return ItemID::Diamond_Ore;
        case BlockID::Ruby_Ore: return ItemID::Ruby_Ore;
        case BlockID::Iron_Block: return ItemID::Iron_Block;
        case BlockID::Gold_Block: return ItemID::Gold_Block;
        case BlockID::Ruby_Block: return ItemID::Ruby_Block;
        case BlockID::Coal_Block: return ItemID::Coal_Block;
        case BlockID::Oak_Log: return ItemID::Oak_Log;
        case BlockID::Oak_Leaves: return ItemID::Oak_Leaves;
        case BlockID::Oak_Planks: return ItemID::Oak_Planks;
        case BlockID::Birch_Log: return ItemID::Birch_Log;
        case BlockID::Birch_Leaves: return ItemID::Birch_Leaves;
        case BlockID::Spruce_Log: return ItemID::Spruce_Log;
        case BlockID::Spruce_Leaves: return ItemID::Spruce_Leaves;
        case BlockID::Woodcutter: return ItemID::Woodcutter;
        case BlockID::Sand: return ItemID::Sand;
        case BlockID::Coarse_Dirt: return ItemID::Coarse_Dirt;
        case BlockID::Bricks: return ItemID::Bricks;
        case BlockID::Furnace: return ItemID::Furnace;
        case BlockID::Workbench: return ItemID::Workbench;
        case BlockID::Gravel: return ItemID::Gravel;
        case BlockID::Cactus: return ItemID::Cactus;
        case BlockID::Dead_Bush: return ItemID::Dead_Bush;
        case BlockID::Smooth_Stone: return ItemID::Smooth_Stone;
        case BlockID::Diorite: return ItemID::Diorite;
        case BlockID::Granite: return ItemID::Granite;
        case BlockID::Glass: return ItemID::Glass;
        case BlockID::Output_Bulb: return ItemID::Output_Bulb;
        case BlockID::Ruby_Wire: return ItemID::Ruby_Wire;
        case BlockID::Coreheart_Of_Blockbit: return ItemID::Coreheart_Of_Blockbit;
        case BlockID::Upgrade_Table: return ItemID::Upgrade_Table;
        default: return ItemID::None;
    }
}

bool canPlaceBlock(World& world, BlockID id, sf::Vector2i position)
{
    auto it = blockDatabase.find(id);
    if (it == blockDatabase.end()) return true;

    const auto& support = it->second.support_blocks_below;
    if (support.empty()) return true;

    Block below = world.getBlock(position.x, position.y - 1);
    return std::find(support.begin(), support.end(), below.id) != support.end();
}

BlockID itemToBlock(ItemID item)
{
    switch(item)
    {
        case ItemID::Stone: return BlockID::Stone;
        case ItemID::Grass: return BlockID::Grass;
        case ItemID::Dirt: return BlockID::Dirt;
        case ItemID::Cobblestone: return BlockID::Cobblestone;
        case ItemID::Obsidian: return BlockID::Obsidian;
        case ItemID::Bedrock: return BlockID::Bedrock;
        case ItemID::Coal_Ore: return BlockID::Coal_Ore;
        case ItemID::Iron_Ore: return BlockID::Iron_Ore;
        case ItemID::Gold_Ore: return BlockID::Gold_Ore;
        case ItemID::Diamond_Ore: return BlockID::Diamond_Ore;
        case ItemID::Ruby_Ore: return BlockID::Ruby_Ore;
        case ItemID::Iron_Block: return BlockID::Iron_Block;
        case ItemID::Gold_Block: return BlockID::Gold_Block;
        case ItemID::Ruby_Block: return BlockID::Ruby_Block;
        case ItemID::Coal_Block: return BlockID::Coal_Block;
        case ItemID::Oak_Log: return BlockID::Oak_Log;
        case ItemID::Oak_Leaves: return BlockID::Oak_Leaves;
        case ItemID::Oak_Planks: return BlockID::Oak_Planks;
        case ItemID::Birch_Log: return BlockID::Birch_Log;
        case ItemID::Birch_Leaves: return BlockID::Birch_Leaves;
        case ItemID::Spruce_Log: return BlockID::Spruce_Log;
        case ItemID::Spruce_Leaves: return BlockID::Spruce_Leaves;
        case ItemID::Woodcutter: return BlockID::Woodcutter;
        case ItemID::Sand: return BlockID::Sand;
        case ItemID::Coarse_Dirt: return BlockID::Coarse_Dirt;
        case ItemID::Snow: return BlockID::Snow;
        case ItemID::Chest: return BlockID::Chest;
        case ItemID::Bricks: return BlockID::Bricks;
        case ItemID::Furnace: return BlockID::Furnace;
        case ItemID::Workbench: return BlockID::Workbench;
        case ItemID::Gravel: return BlockID::Gravel;
        case ItemID::Cactus: return BlockID::Cactus;
        case ItemID::Dead_Bush: return BlockID::Dead_Bush;
        case ItemID::Smooth_Stone: return BlockID::Smooth_Stone;
        case ItemID::Diorite: return BlockID::Diorite;
        case ItemID::Granite: return BlockID::Granite;
        case ItemID::Glass: return BlockID::Glass;
        case ItemID::Output_Bulb: return BlockID::Output_Bulb;
        case ItemID::Ruby_Wire: return BlockID::Ruby_Wire;
        case ItemID::Coreheart_Of_Blockbit: return BlockID::Coreheart_Of_Blockbit;
        case ItemID::Upgrade_Table: return BlockID::Upgrade_Table;
        default: return BlockID::Air;
    }
}


// blockDatabase is now populated by PackLoader at startup from
// resources/packs/blockbit/blocks/*.json. The old static initializer
// was removed in Phase 1c-E — see registerVanillaContent().
std::unordered_map<BlockID, BlockData> blockDatabase;
