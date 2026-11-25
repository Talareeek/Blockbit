#include "../include/Block.hpp"
#include "../include/BlockUpdatePipeline.hpp"
#include "../include/ServerWorld.hpp"
#include "../include/Prefab.hpp"

#include <algorithm>
#include <random>

// Vanilla block callbacks (named, not anonymous) so ContentProfiles can wire
// them into pack-loaded blocks by name. See include/ContentProfiles.hpp.
namespace blockbit_callbacks
{
    // Can a falling block pass through this cell on the way down? Air, water
    // and lava are passable; the fluid gets displaced by the final landing.
    bool falling_block_can_leave_into(Block b)
    {
        return b.id == Blocks::Air || b.id == Blocks::Water || b.id == Blocks::Lava;
    }

    bool is_falling_block(BlockKey id)
    {
        return id == Blocks::Sand || id == Blocks::Gravel;
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
        ctx.world.setBlock(pos.x, pos.y, {Blocks::Air, 0});
        spawnFallingBlock(
            ctx.world,
            {static_cast<double>(pos.x), static_cast<double>(pos.y)},
            self.id,
            self.metadata
        );
    }

    // Delay between a water cell being disturbed and its flow logic firing.
    // Keep this in sync with the default pack's water tick delay.
    // Used as the fallback when a fluid's CascadeFluid behavior doesn't set
    // tick_delay_ticks.
    constexpr int WATER_TICK_DELAY = 5;

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
        return b.id == Blocks::Air || b.id == Blocks::Water;
    }

    // A non-source cell survives only if something is actively supplying it:
    // water directly above (vertical column), or a stronger neighbour sideways.
    // Without that, it evaporates — same decay rule the old sweep used.
    bool water_is_fed(World& world, sf::Vector2i pos, uint8_t level)
    {
        if (level == WaterState::SOURCE) return true;

        if (world.getBlock(pos.x, pos.y + 1).id == Blocks::Water) return true;

        Block left  = world.getBlock(pos.x - 1, pos.y);
        Block right = world.getBlock(pos.x + 1, pos.y);
        if (left.id  == Blocks::Water && WaterState::unpack(left.metadata).level  > level) return true;
        if (right.id == Blocks::Water && WaterState::unpack(right.metadata).level > level) return true;
        return false;
    }

    void water_schedule_self(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        if (ctx.world.getBlock(pos.x, pos.y).id != Blocks::Water) return;
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
        if (self.id != Blocks::Water) return;

        const uint8_t level = WaterState::unpack(self.metadata).level;

        // Air-ing the cell fires NC on neighbours, which reschedules the next
        // weaker water in the chain — the decay cascade unwinds one tick-delay
        // at a time instead of in a single global sweep.
        if (!water_is_fed(ctx.world, pos, level))
        {
            ctx.world.setBlock(pos.x, pos.y, {Blocks::Air, 0});
            return;
        }

        Block below = ctx.world.getBlock(pos.x, pos.y - 1);

        // Free-fall into air always writes FULL regardless of our own level —
        // falling water keeps its strength until it hits something to spread on.
        if (below.id == Blocks::Air)
        {
            ctx.world.setBlock(pos.x, pos.y - 1, {Blocks::Water, WaterState{WaterState::FULL}.pack()});
            return;
        }

        // Water already underneath us: settled column, let that cell handle
        // whatever sideways propagation happens from the bottom.
        if (below.id == Blocks::Water) return;

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
            // Water flowing into lava reacts with the lava cell. The water
            // remains in its current cell; flowing lava becomes cobblestone,
            // while a lava source becomes obsidian.
            if (target.id == Blocks::Lava)
            {
                const bool source = LavaState::unpack(target.metadata).level == LavaState::SOURCE;
                ctx.world.setBlock(tx, ty, {source ? Blocks::Obsidian : Blocks::Cobblestone, 0});
                return;
            }
            if (!water_cell_passable(target)) return;
            if (target.id == Blocks::Water && WaterState::unpack(target.metadata).level >= new_level) return;
            ctx.world.setBlock(tx, ty, {Blocks::Water, WaterState{new_level}.pack()});
        };

        try_flow(pos.x - 1, pos.y);
        try_flow(pos.x + 1, pos.y);
    }

    constexpr int CACTUS_MAX_HEIGHT = 3;

    void cactus_on_post_placement(BlockUpdateContext& ctx, sf::Vector2i pos, uint8_t /*from_direction*/)
    {
        Block self = ctx.world.getBlock(pos.x, pos.y);
        if (self.id != Blocks::Cactus) return;

        // Support (sand/cactus below) is handled by BlockData::support_blocks_below
        // via the pipeline — this callback only reconciles the is_top flag so
        // the mesh picks up the correct texture when the column changes.
        const bool should_be_top = ctx.world.getBlock(pos.x, pos.y + 1).id != Blocks::Cactus;
        const bool is_top = CactusState::unpack(self.metadata).is_top;
        if (should_be_top != is_top)
        {
            ctx.pipeline.setBlockSuppressNC(pos, {Blocks::Cactus, CactusState{should_be_top}.pack()});
        }
    }

    void cactus_on_random_tick(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        Block self = ctx.world.getBlock(pos.x, pos.y);
        if (self.id != Blocks::Cactus) return;

        // Growth is driven from the topmost cell so a 2-tall cactus only picks
        // up one random tick per sampling instead of one per segment.
        if (!CactusState::unpack(self.metadata).is_top) return;

        int height = 1;
        for (int y = pos.y - 1; y >= 0; --y)
        {
            if (ctx.world.getBlock(pos.x, y).id != Blocks::Cactus) break;
            ++height;
        }

        // Read max_height and grows_as from the block's PlantGrowth behavior
        // so modpack-defined plants with different caps / species go through
        // the same growth pipeline.
        int max_height = CACTUS_MAX_HEIGHT;
        BlockKey grown  = Blocks::Cactus;
        if (auto it = blockDatabase.find(self.id); it != blockDatabase.end())
        {
            const auto& bh = it->second.behavior;
            if (bh.type == BlockBehaviorType::PlantGrowth)
                if (const auto* p = std::get_if<PlantGrowthParams>(&bh.params))
                {
                    if (p->max_height > 0) max_height = p->max_height;
                    if (p->grows_as != BlockKey{}) grown = p->grows_as;
                }
        }

        if (height >= max_height) return;

        Block above = ctx.world.getBlock(pos.x, pos.y + 1);
        if (above.id != Blocks::Air) return;

        // Suppress NC so neighbouring sand/water don't fire logical updates for
        // what is logically a self-contained growth step.
        ctx.pipeline.setBlockSuppressNC(pos, {grown, CactusState{false}.pack()});
        ctx.pipeline.setBlockSuppressNC({pos.x, pos.y + 1}, {grown, CactusState{true}.pack()});
    }

    void sapling_on_random_tick(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        const BlockKey sapling = ctx.world.getBlock(pos.x, pos.y).id;
        BlockKey log, leaves;
        if (sapling == "blockbit:oak_sapling") { log = Blocks::Oak_Log; leaves = Blocks::Oak_Leaves; }
        else if (sapling == "blockbit:birch_sapling") { log = Blocks::Birch_Log; leaves = Blocks::Birch_Leaves; }
        else if (sapling == "blockbit:spruce_sapling") { log = Blocks::Spruce_Log; leaves = Blocks::Spruce_Leaves; }
        else return;
        if (ctx.world.getBlock(pos.x, pos.y - 1).id != Blocks::Dirt &&
            ctx.world.getBlock(pos.x, pos.y - 1).id != Blocks::Grass) return;
        // The sapling occupies pos itself; only the three blocks above it
        // need to be clear for this four-block-tall tree.
        for (int y = 1; y <= 3; ++y)
            if (ctx.world.getBlock(pos.x, pos.y + y).id != Blocks::Air) return;
        ctx.pipeline.setBlockSuppressNC(pos, {log, 0});
        for (int y = 1; y <= 3; ++y) ctx.pipeline.setBlockSuppressNC({pos.x, pos.y + y}, {log, 0});
        for (int dx = -2; dx <= 2; ++dx)
            for (int dy = 2; dy <= 4; ++dy)
                if ((std::abs(dx) + std::abs(dy - 3) <= 3) && ctx.world.getBlock(pos.x + dx, pos.y + dy).id == Blocks::Air)
                    ctx.pipeline.setBlockSuppressNC({pos.x + dx, pos.y + dy}, {leaves, 0});
    }

    // Leaves stay alive while a matching trunk is within four blocks. Random
    // ticks make detached canopies decay over time without a separate sweep.
    void leaves_on_random_tick(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        const BlockKey leaf = ctx.world.getBlock(pos.x, pos.y).id;
        BlockKey log = Blocks::Air;
        if (leaf == Blocks::Oak_Leaves) log = Blocks::Oak_Log;
        else if (leaf == Blocks::Birch_Leaves) log = Blocks::Birch_Log;
        else if (leaf == Blocks::Spruce_Leaves) log = Blocks::Spruce_Log;
        else return;

        for (int dx = -4; dx <= 4; ++dx)
        {
            for (int dy = -4; dy <= 4; ++dy)
            {
                if (ctx.world.getBlock(pos.x + dx, pos.y + dy).id == log) return;
            }
        }

        // Detached leaves occasionally leave a matching sapling behind.
        auto data = blockDatabase.find(leaf);
        std::mt19937 rng(std::random_device{}());
        if (data != blockDatabase.end() && data->second.drop && data->second.drop->alternative_drop_chance > 0.0f &&
            std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < data->second.drop->alternative_drop_chance)
        {
            spawnDroppedItem(ctx.world, {pos.x + 0.25, pos.y + 0.25}, ItemStack{data->second.drop->alternative_drop, 1});
        }
        ctx.world.setBlock(pos.x, pos.y, {Blocks::Air, 0});
    }

    // Lava uses the same cascade as water but ticks six times slower and
    // caps horizontal reach at ~3 cells (half of water's ~7). Levels follow
    // LavaState (FULL=4, SOURCE=5) instead of WaterState.
    constexpr int LAVA_TICK_DELAY = 30;

    bool lava_cell_passable(Block b)
    {
        return b.id == Blocks::Air || b.id == Blocks::Lava;
    }

    bool lava_is_fed(World& world, sf::Vector2i pos, uint8_t level)
    {
        if (level == LavaState::SOURCE) return true;

        if (world.getBlock(pos.x, pos.y + 1).id == Blocks::Lava) return true;

        Block left  = world.getBlock(pos.x - 1, pos.y);
        Block right = world.getBlock(pos.x + 1, pos.y);
        if (left.id  == Blocks::Lava && LavaState::unpack(left.metadata).level  > level) return true;
        if (right.id == Blocks::Lava && LavaState::unpack(right.metadata).level > level) return true;
        return false;
    }

    void lava_schedule_self(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        if (ctx.world.getBlock(pos.x, pos.y).id != Blocks::Lava) return;
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
        if (self.id != Blocks::Lava) return;

        const uint8_t level = LavaState::unpack(self.metadata).level;

        if (!lava_is_fed(ctx.world, pos, level))
        {
            ctx.world.setBlock(pos.x, pos.y, {Blocks::Air, 0});
            return;
        }

        Block below = ctx.world.getBlock(pos.x, pos.y - 1);

        if (below.id == Blocks::Air)
        {
            ctx.world.setBlock(pos.x, pos.y - 1, {Blocks::Lava, LavaState{LavaState::FULL}.pack()});
            return;
        }

        if (below.id == Blocks::Lava) return;

        if (level <= 1) return;

        const uint8_t new_level = (level < LavaState::SOURCE)
            ? static_cast<uint8_t>(level - 1)
            : static_cast<uint8_t>(LavaState::FULL - 1);

        auto try_flow = [&](int tx, int ty)
        {
            Block target = ctx.world.getBlock(tx, ty);
            if (!lava_cell_passable(target)) return;
            if (target.id == Blocks::Lava && LavaState::unpack(target.metadata).level >= new_level) return;
            ctx.world.setBlock(tx, ty, {Blocks::Lava, LavaState{new_level}.pack()});
        };

        try_flow(pos.x - 1, pos.y);
        try_flow(pos.x + 1, pos.y);
    }

    void lava_on_post_placement(BlockUpdateContext& ctx, sf::Vector2i pos, uint8_t /*from_direction*/)
    {
        Block self = ctx.world.getBlock(pos.x, pos.y);
        if (self.id != Blocks::Lava) return;

        bool source = LavaState::unpack(self.metadata).level == LavaState::SOURCE;

        Block top = ctx.world.getBlock(pos.x, pos.y + 1);
        Block bottom = ctx.world.getBlock(pos.x, pos.y - 1);
        Block left = ctx.world.getBlock(pos.x - 1, pos.y);
        Block right = ctx.world.getBlock(pos.x + 1, pos.y);

        if(top.id == Blocks::Water || left.id == Blocks::Water || right.id == Blocks::Water) ctx.world.setBlock(pos.x, pos.y, Block{(source) ? Blocks::Obsidian : Blocks::Cobblestone});
    }

    void water_on_post_placement(BlockUpdateContext& ctx, sf::Vector2i pos, uint8_t /*from_direction*/)
    {
        constexpr sf::Vector2i offsets[] = {{0, 1}, {0, -1}, {-1, 0}, {1, 0}};
        for (const sf::Vector2i offset : offsets)
        {
            const sf::Vector2i lava_pos = pos + offset;
            const Block lava = ctx.world.getBlock(lava_pos.x, lava_pos.y);
            if (lava.id != Blocks::Lava) continue;

            const bool source = LavaState::unpack(lava.metadata).level == LavaState::SOURCE;
            ctx.world.setBlock(lava_pos.x, lava_pos.y,
                               {source ? Blocks::Obsidian : Blocks::Cobblestone, 0});
        }
    }
}

ItemKey blockToItem(BlockKey block)
{
    static const std::unordered_map<BlockKey, ItemKey> block_items = {
        {Blocks::Stone, Items::Stone}, {Blocks::Grass, Items::Grass},
        {Blocks::Dirt, Items::Dirt}, {Blocks::Cobblestone, Items::Cobblestone},
        {Blocks::Obsidian, Items::Obsidian}, {Blocks::Bedrock, Items::Bedrock},
        {Blocks::Coal_Ore, Items::Coal_Ore}, {Blocks::Iron_Ore, Items::Iron_Ore},
        {Blocks::Gold_Ore, Items::Gold_Ore}, {Blocks::Diamond_Ore, Items::Diamond_Ore},
        {Blocks::Ruby_Ore, Items::Ruby_Ore}, {Blocks::Iron_Block, Items::Iron_Block},
        {Blocks::Gold_Block, Items::Gold_Block}, {Blocks::Ruby_Block, Items::Ruby_Block},
        {Blocks::Coal_Block, Items::Coal_Block}, {Blocks::Oak_Log, Items::Oak_Log},
        {Blocks::Oak_Leaves, Items::Oak_Leaves}, {Blocks::Oak_Planks, Items::Oak_Planks},
        {Blocks::Birch_Planks, Items::Birch_Planks}, {Blocks::Spruce_Planks, Items::Spruce_Planks},
        {Blocks::Copper_Ore, Items::Copper_Ore}, {Blocks::Copper_Block, Items::Copper_Block},
        {Blocks::Birch_Log, Items::Birch_Log}, {Blocks::Birch_Leaves, Items::Birch_Leaves},
        {Blocks::Spruce_Log, Items::Spruce_Log}, {Blocks::Spruce_Leaves, Items::Spruce_Leaves},
        {Blocks::Woodcutter, Items::Woodcutter}, {Blocks::Sand, Items::Sand},
        {Blocks::Coarse_Dirt, Items::Coarse_Dirt}, {Blocks::Bricks, Items::Bricks},
        {Blocks::Furnace, Items::Furnace}, {Blocks::Workbench, Items::Workbench},
        {Blocks::Gravel, Items::Gravel}, {Blocks::Cactus, Items::Cactus},
        {Blocks::Dead_Bush, Items::Dead_Bush}, {Blocks::Smooth_Stone, Items::Smooth_Stone},
        {Blocks::Diorite, Items::Diorite}, {Blocks::Granite, Items::Granite},
        {Blocks::Glass, Items::Glass}, {Blocks::Output_Bulb, Items::Output_Bulb},
        {Blocks::Ruby_Wire, Items::Ruby_Wire},
        {Blocks::Coreheart_Of_Blockbit, Items::Coreheart_Of_Blockbit},
        {Blocks::Upgrade_Table, Items::Upgrade_Table}
    };
    auto it = block_items.find(block);
    if (it != block_items.end()) return it->second;
    return itemDatabase.contains(block) ? block : Items::None;
}

bool canPlaceBlock(World& world, BlockKey id, sf::Vector2i position)
{
    auto it = blockDatabase.find(id);
    if (it == blockDatabase.end()) return true;

    const auto& support = it->second.support_blocks_below;
    if (support.empty()) return true;

    Block below = world.getBlock(position.x, position.y - 1);
    return std::find(support.begin(), support.end(), below.id) != support.end();
}

BlockKey itemToBlock(ItemKey item)
{
    static const std::unordered_map<ItemKey, BlockKey> item_blocks = {
        {Items::Stone, Blocks::Stone}, {Items::Grass, Blocks::Grass},
        {Items::Dirt, Blocks::Dirt}, {Items::Cobblestone, Blocks::Cobblestone},
        {Items::Obsidian, Blocks::Obsidian}, {Items::Bedrock, Blocks::Bedrock},
        {Items::Coal_Ore, Blocks::Coal_Ore}, {Items::Iron_Ore, Blocks::Iron_Ore},
        {Items::Gold_Ore, Blocks::Gold_Ore}, {Items::Diamond_Ore, Blocks::Diamond_Ore},
        {Items::Ruby_Ore, Blocks::Ruby_Ore}, {Items::Iron_Block, Blocks::Iron_Block},
        {Items::Gold_Block, Blocks::Gold_Block}, {Items::Ruby_Block, Blocks::Ruby_Block},
        {Items::Coal_Block, Blocks::Coal_Block}, {Items::Oak_Log, Blocks::Oak_Log},
        {Items::Oak_Leaves, Blocks::Oak_Leaves}, {Items::Oak_Planks, Blocks::Oak_Planks},
        {Items::Birch_Planks, Blocks::Birch_Planks}, {Items::Spruce_Planks, Blocks::Spruce_Planks},
        {Items::Copper_Ore, Blocks::Copper_Ore}, {Items::Copper_Block, Blocks::Copper_Block},
        {Items::Birch_Log, Blocks::Birch_Log}, {Items::Birch_Leaves, Blocks::Birch_Leaves},
        {Items::Spruce_Log, Blocks::Spruce_Log}, {Items::Spruce_Leaves, Blocks::Spruce_Leaves},
        {Items::Woodcutter, Blocks::Woodcutter}, {Items::Sand, Blocks::Sand},
        {Items::Coarse_Dirt, Blocks::Coarse_Dirt}, {Items::Snow, Blocks::Snow},
        {Items::Chest, Blocks::Chest}, {Items::Bricks, Blocks::Bricks},
        {Items::Furnace, Blocks::Furnace}, {Items::Workbench, Blocks::Workbench},
        {Items::Gravel, Blocks::Gravel}, {Items::Cactus, Blocks::Cactus},
        {Items::Dead_Bush, Blocks::Dead_Bush}, {Items::Smooth_Stone, Blocks::Smooth_Stone},
        {Items::Diorite, Blocks::Diorite}, {Items::Granite, Blocks::Granite},
        {Items::Glass, Blocks::Glass}, {Items::Output_Bulb, Blocks::Output_Bulb},
        {Items::Ruby_Wire, Blocks::Ruby_Wire},
        {Items::Coreheart_Of_Blockbit, Blocks::Coreheart_Of_Blockbit},
        {Items::Upgrade_Table, Blocks::Upgrade_Table}
    };
    auto it = item_blocks.find(item);
    if (it != item_blocks.end()) return it->second;
    return blockDatabase.contains(item) ? item : Blocks::Air;
}


// blockDatabase is now populated by PackLoader at startup from
// resources/packs/blockbit/blocks/*.json. The old static initializer
// was removed in Phase 1c-E — see registerVanillaContent().
std::unordered_map<BlockKey, BlockData> blockDatabase;
