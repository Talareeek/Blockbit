#include "../include/Block.hpp"
#include "../include/BlockUpdatePipeline.hpp"
#include "../include/ServerWorld.hpp"
#include "../include/Prefab.hpp"

namespace
{
    // Can a falling block pass through this cell on the way down? Air and water
    // are both passable; water gets displaced by the final landing.
    bool falling_block_can_leave_into(Block b)
    {
        return b.id == BlockID::Air || b.id == BlockID::Water;
    }

    void sand_try_schedule_fall(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        Block below = ctx.world.getBlock(pos.x, pos.y - 1);
        if (!falling_block_can_leave_into(below)) return;
        ctx.pipeline.scheduleTick(pos, 2);  // 2-tick delay matches vanilla sand
    }

    void sand_on_place(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        sand_try_schedule_fall(ctx, pos);
    }

    void sand_on_post_placement(BlockUpdateContext& ctx, sf::Vector2i pos, uint8_t /*from_direction*/)
    {
        // A neighbour change may have opened a hole below us. Only reschedule
        // if we're still sand and we don't already have a pending tick.
        if (ctx.world.getBlock(pos.x, pos.y).id != BlockID::Sand) return;
        if (ctx.pipeline.hasScheduledTick(pos)) return;
        sand_try_schedule_fall(ctx, pos);
    }

    void sand_on_scheduled_tick(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        Block self = ctx.world.getBlock(pos.x, pos.y);
        if (self.id != BlockID::Sand) return;

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
    constexpr int WATER_TICK_DELAY = 30;

    bool water_cell_passable(Block b)
    {
        return b.id == BlockID::Air || b.id == BlockID::Water;
    }

    // A non-source cell survives only if something is actively supplying it:
    // water directly above (vertical column), or a stronger neighbour sideways.
    // Without that, it evaporates — same decay rule the old sweep used.
    bool water_is_fed(World& world, sf::Vector2i pos, uint8_t level)
    {
        if (level == static_cast<uint8_t>(WaterLevel::SOURCE)) return true;

        if (world.getBlock(pos.x, pos.y + 1).id == BlockID::Water) return true;

        Block left  = world.getBlock(pos.x - 1, pos.y);
        Block right = world.getBlock(pos.x + 1, pos.y);
        if (left.id  == BlockID::Water && left.metadata  > level) return true;
        if (right.id == BlockID::Water && right.metadata > level) return true;
        return false;
    }

    void water_schedule_self(BlockUpdateContext& ctx, sf::Vector2i pos)
    {
        if (ctx.world.getBlock(pos.x, pos.y).id != BlockID::Water) return;
        if (ctx.pipeline.hasScheduledTick(pos)) return;
        ctx.pipeline.scheduleTick(pos, WATER_TICK_DELAY);
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

        const uint8_t level  = self.metadata;
        const uint8_t SOURCE = static_cast<uint8_t>(WaterLevel::SOURCE);
        const uint8_t FULL   = static_cast<uint8_t>(WaterLevel::FULL);

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
            ctx.world.setBlock(pos.x, pos.y - 1, {BlockID::Water, FULL});
            return;
        }

        // Water already underneath us: settled column, let that cell handle
        // whatever sideways propagation happens from the bottom.
        if (below.id == BlockID::Water) return;

        // Level 1 is the floor — no lower level to project sideways.
        if (level <= 1) return;

        // SOURCE and FULL both project level 7 sideways; everything else
        // decreases by one, matching the old sweep's reach.
        const uint8_t new_level = (level < SOURCE)
            ? static_cast<uint8_t>(level - 1)
            : static_cast<uint8_t>(FULL - 1);

        auto try_flow = [&](int tx, int ty)
        {
            Block target = ctx.world.getBlock(tx, ty);
            if (!water_cell_passable(target)) return;
            if (target.id == BlockID::Water && target.metadata >= new_level) return;
            ctx.world.setBlock(tx, ty, {BlockID::Water, new_level});
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
        case BlockID::Woodcutter: return ItemID::Woodcutter;
        case BlockID::Sand: return ItemID::Sand;
        case BlockID::Coarse_Dirt: return ItemID::Coarse_Dirt;
        case BlockID::Bricks: return ItemID::Bricks;
        case BlockID::Furnace: return ItemID::Furnace;
        case BlockID::Workbench: return ItemID::Workbench;
        default: return ItemID::None;
    }
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
        case ItemID::Woodcutter: return BlockID::Woodcutter;
        case ItemID::Sand: return BlockID::Sand;
        case ItemID::Snow: return BlockID::Snow;
        case ItemID::Chest: return BlockID::Chest;
        case ItemID::Bricks: return BlockID::Bricks;
        case ItemID::Furnace: return BlockID::Furnace;
        case ItemID::Workbench: return BlockID::Workbench;
        default: return BlockID::Air;
    }
}

std::unordered_map<BlockID, BlockData> blockDatabase = 
{
    { BlockID::Air,
        {
            .solid = false,
            .transparent = true,
            .breakable = false,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 0.0,
        }
    },
    { BlockID::Stone,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 1.5f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Stone, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 1}},
            .drop = DropProperties{.drop = ItemID::Cobblestone}
        }
    },
    { BlockID::Grass,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 0.6f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Grass, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 2, .desired_tool = {.tool_type = ToolType::Shovel, .level = 1}},
            .drop = DropProperties{.always_drop = true, .drop = ItemID::Dirt}
        }
    },
    { BlockID::Dirt,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 0.5f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Dirt, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 2, .desired_tool = {.tool_type = ToolType::Shovel, .level = 1}},
            .drop = DropProperties{.always_drop = true, .drop = ItemID::Dirt}
        }
    },
    { BlockID::Cobblestone,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 2.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Cobblestone, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 1}},
            .drop = DropProperties{.drop = ItemID::Cobblestone}
        }
    },
    { BlockID::Obsidian,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 50.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Obsidian, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 50, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 5}},
            .drop = DropProperties{.drop = ItemID::Obsidian}
        }
    },
    { BlockID::Bedrock,
        {
            .solid = true,
            .transparent = false,
            .breakable = false,
            .liquid = false,
            .drag = 1.0f,
            .hardness = -1.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Bedrock, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = INT_MAX, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = INT_MAX}}
        }
    },
    { BlockID::Water,
        {
            .solid = false,
            .transparent = true,
            .breakable = false,
            .liquid = true,
            .drag = 4.0f,
            .hardness = 0.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Water, .render_bounds = water_render_bounds, .rect = water_render_rect},
            .callbacks = {
                .onPlace            = water_on_place,
                .onNeighborChanged  = water_on_neighbor_changed,
                .onScheduledTick    = water_on_scheduled_tick
            }
        }
    },
    { BlockID::Coal_Ore,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 2.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Coal_Ore, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 1}},
            .drop = DropProperties{.drop = ItemID::Coal}
        }
    },
    { BlockID::Iron_Ore,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 3.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Iron_Ore, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 2}},
            .drop = DropProperties{.drop = ItemID::Raw_Iron}
        }
    },
    { BlockID::Gold_Ore,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 3.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Gold_Ore, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 4}},
            .drop = DropProperties{.drop = ItemID::Raw_Gold}
        }
    },
    { BlockID::Diamond_Ore,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 5.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Diamond_Ore, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 4}},
            .drop = DropProperties{.drop = ItemID::Diamond}
        }
    },
    { BlockID::Ruby_Ore,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 5.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Ruby_Ore, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 6, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 5}},
            .drop = DropProperties{.drop = ItemID::Ruby}
        }
    },
    { BlockID::Iron_Block,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 5.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Iron_Block, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 6, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 2}},
            .drop = DropProperties{.drop = ItemID::Iron_Block}
        }
    },
    { BlockID::Gold_Block,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 4.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Gold_Block, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 6, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 4}},
            .drop = DropProperties{.drop = ItemID::Gold_Block}
        }
    },
    { BlockID::Ruby_Block,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 6.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Ruby_Block, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 7, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 5}},
            .drop = DropProperties{.drop = ItemID::Ruby_Block}
        }
    },
    { BlockID::Coal_Block,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 3.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Coal_Block, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 1}},
            .drop = DropProperties{.drop = ItemID::Coal_Block}
        }
    },
    { BlockID::Oak_Log,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 2.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Oak_Log, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Axe, .level = 1}},
            .drop = DropProperties{.always_drop = true, .drop = ItemID::Oak_Log}
        }
    },
    { BlockID::Oak_Leaves,
        {
            .solid = true,
            .transparent = true,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 0.2f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Oak_Leaves, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Axe, .level = 1}},
            .drop = DropProperties{.always_drop = true, .drop = ItemID::Oak_Leaves}
        }
    },
    { BlockID::Oak_Planks,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 0.2f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Oak_Planks, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Axe, .level = 1}},
            .drop = DropProperties{.always_drop = true, .drop = ItemID::Oak_Planks}
        }
    },
    { BlockID::Woodcutter,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 2.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Woodcutter, .render_bounds = default_render_bounds, .rect = woodcutter_render_rect, .animation = woodcutter_animation},
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Axe, .level = 1}},
            .drop = DropProperties{.always_drop = true, .drop = ItemID::Woodcutter}
        }
    },
    { BlockID::Fire,
        {
            .solid = false,
            .transparent = true,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 0.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Fire, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 0, .desired_tool = {.tool_type = ToolType::Axe, .level = 1}}
        }
    },
    { BlockID::Sand,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 0.5f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Sand, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 2, .desired_tool = {.tool_type = ToolType::Shovel, .level = 1}},
            .drop = DropProperties{.always_drop = true, .drop = ItemID::Sand},
            .callbacks = {
                .onPlace            = sand_on_place,
                .onPostPlacement    = sand_on_post_placement,
                .onScheduledTick    = sand_on_scheduled_tick
            }
        }
    },
    { BlockID::Coarse_Dirt,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 0.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Coarse_Dirt, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 2, .desired_tool = {.tool_type = ToolType::Shovel, .level = 1}},
            .drop = DropProperties{.always_drop = true, .drop = ItemID::Coarse_Dirt}
        }
    },
    { BlockID::Snow,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 0.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Snow, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 1, .desired_tool = {.tool_type = ToolType::Shovel, .level = 1}},
            .drop = DropProperties{.always_drop = true, .drop = ItemID::Snow}
        }
    },
    { BlockID::Chest,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 2.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Chest, .render_bounds = chest_render_bounds, .rect = chest_render_rect},
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Axe, .level = 1}},
            .drop = DropProperties{.always_drop = true, .drop = ItemID::Chest}
        }
    },
    { BlockID::Bricks,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 2.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Bricks, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 1}},
            .drop = DropProperties{.drop = ItemID::Bricks}
        }
    },
    { BlockID::Furnace,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 3.5f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Furnace, .render_bounds = furnace_render_bounds, .rect = furnace_render_rect, .animation = furnace_animation},
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 1}},
            .drop = DropProperties{.drop = ItemID::Furnace}
        }
    },
    { BlockID::Workbench,
        {
            .solid = true,
            .transparent = false,
            .breakable = true,
            .liquid = false,
            .drag = 1.0f,
            .hardness = 2.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Workbench, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Axe, .level = 1}},
            .drop = DropProperties{.always_drop = true, .drop = ItemID::Workbench}
        }
    },
};