#ifndef BLOCK_HPP
#define BLOCK_HPP

#include <cstdint>
#include <unordered_map>
#include <climits>
#include <optional>
#include "Item.hpp"
#include "AssetManager.hpp"

enum class BlockID : uint32_t
{
    Air,
    Stone,
    Grass,
    Dirt,
    Cobblestone,
    Obsidian,
    Bedrock,
    Water,
    Iron_Ore,
    Gold_Ore,
    Diamond_Ore,
    Ruby_Ore,
    Oak_Log,
    Oak_Leaves,
    Oak_Planks,
    Woodcutter,
    Fire,
    Sand,
    Coarse_Dirt,
    Snow
};

struct MiningProperties
{
    int mining_resistance;

    ToolProperties desired_tool;
};

struct DropProperties
{
    bool always_drop = false;
    ItemID drop;

    uint8_t min_amount = 1;
    uint8_t max_amount = 1;
};

struct BlockData
{
    bool solid;
    bool transparent;
    bool breakable;

    bool liquid = false;
    float drag = 1.0f;

    float hardness;
    AssetManager::GameTextureID texture;

    std::optional<MiningProperties> mining;
    std::optional<DropProperties> drop;
};

struct Block
{
    BlockID id;
    uint8_t metadata;
};

inline std::unordered_map<BlockID, BlockData> blockDatabase = 
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
            .texture = AssetManager::GameTextureID::Stone,
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
            .texture = AssetManager::GameTextureID::Grass,
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
            .texture = AssetManager::GameTextureID::Dirt,
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
            .texture = AssetManager::GameTextureID::Cobblestone,
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
            .texture = AssetManager::GameTextureID::Obsidian,
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
            .texture = AssetManager::GameTextureID::Bedrock,
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
            .texture = AssetManager::GameTextureID::Water
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
            .texture = AssetManager::GameTextureID::Iron_Ore,
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
            .texture = AssetManager::GameTextureID::Gold_Ore,
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
            .texture = AssetManager::GameTextureID::Diamond_Ore,
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
            .texture = AssetManager::GameTextureID::Ruby_Ore,
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
            .texture = AssetManager::GameTextureID::Oak_Log,
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
            .texture = AssetManager::GameTextureID::Oak_Leaves,
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
            .texture = AssetManager::GameTextureID::Oak_Planks,
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
            .texture = AssetManager::GameTextureID::Woodcutter,
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
            .texture = AssetManager::GameTextureID::Fire,
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
            .texture = AssetManager::GameTextureID::Sand,
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
            .texture = AssetManager::GameTextureID::Coarse_Dirt,
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
            .texture = AssetManager::GameTextureID::Snow,
            .mining = MiningProperties{.mining_resistance = 1, .desired_tool = {.tool_type = ToolType::Shovel, .level = 1}},
            .drop = DropProperties{.drop = ItemID::Snow}
        }
    }
};


enum class WaterLevel : uint8_t
{
    FULL = 8,
    SOURCE = 9
};

extern ItemID blockToItem(BlockID block);

extern BlockID itemToBlock(ItemID item);

#endif // BLOCK_HPP