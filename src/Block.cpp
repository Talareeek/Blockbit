#include "../include/Block.hpp"

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
        case BlockID::Iron_Ore: return ItemID::Iron_Ore;
        case BlockID::Gold_Ore: return ItemID::Gold_Ore;
        case BlockID::Diamond_Ore: return ItemID::Diamond_Ore;
        case BlockID::Ruby_Ore: return ItemID::Ruby_Ore;
        case BlockID::Oak_Log: return ItemID::Oak_Log;
        case BlockID::Oak_Leaves: return ItemID::Oak_Leaves;
        case BlockID::Oak_Planks: return ItemID::Oak_Planks;
        case BlockID::Woodcutter: return ItemID::Woodcutter;
        case BlockID::Sand: return ItemID::Sand;
        case BlockID::Coarse_Dirt: return ItemID::Coarse_Dirt;
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
        case ItemID::Iron_Ore: return BlockID::Iron_Ore;
        case ItemID::Gold_Ore: return BlockID::Gold_Ore;
        case ItemID::Diamond_Ore: return BlockID::Diamond_Ore;
        case ItemID::Ruby_Ore: return BlockID::Ruby_Ore;
        case ItemID::Oak_Log: return BlockID::Oak_Log;
        case ItemID::Oak_Leaves: return BlockID::Oak_Leaves;
        case ItemID::Oak_Planks: return BlockID::Oak_Planks;
        case ItemID::Woodcutter: return BlockID::Woodcutter;
        case ItemID::Sand: return BlockID::Sand;
        case ItemID::Snow: return BlockID::Snow;
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
            .drop = DropProperties{.drop = ItemID::Dirt}
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
            .drop = DropProperties{.drop = ItemID::Dirt}
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
            .drop = DropProperties{.drop = ItemID::Iron_Ore}
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
            .drop = DropProperties{.drop = ItemID::Gold_Ore}
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
            .drop = DropProperties{.drop = ItemID::Diamond_Ore}
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
            .drop = DropProperties{.drop = ItemID::Oak_Log}
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
            .drop = DropProperties{.drop = ItemID::Oak_Leaves}
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
            .drop = DropProperties{.drop = ItemID::Oak_Planks}
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
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Woodcutter, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Axe, .level = 1}},
            .drop = DropProperties{.drop = ItemID::Woodcutter}
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
            .hardness = 0.0f,
            .render = RenderProperties{.texture = AssetManager::GameTextureID::Sand, .render_bounds = default_render_bounds, .rect = default_render_rect},
            .mining = MiningProperties{.mining_resistance = 2, .desired_tool = {.tool_type = ToolType::Shovel, .level = 1}},
            .drop = DropProperties{.drop = ItemID::Sand}
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
            .drop = DropProperties{.drop = ItemID::Coarse_Dirt}
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
            .drop = DropProperties{.drop = ItemID::Snow}
        }
    }
};