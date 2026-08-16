#ifndef BLOCK_HPP
#define BLOCK_HPP

#include <cstdint>
#include <unordered_map>
#include <climits>
#include <optional>
#include "Item.hpp"

enum class BlockID : uint32_t
{
    Air = 0,
    Stone = 1,
    Grass = 2,
    Dirt = 3,
    Cobblestone = 4,
    Obsidian = 5,
    Bedrock = 6,
    Water = 7,
    Iron_Ore = 8,
    Gold_Ore = 9,
    Diamond_Ore = 10,
    Oak_Log = 11,
    Oak_Leaves = 12,
    Woodcutter = 13,
    Fire = 14,
    Sand = 15,
    Coarse_Dirt = 16,
    Snow = 17
};

struct MiningProperties
{
    int mining_resistance;

    ToolProperties desired_tool;
};

struct BlockData
{
    bool solid;
    bool transparent;
    bool breakable;

    bool liquid = false;
    float drag = 1.0f;

    float hardness;
    uint32_t texture;

    std::optional<MiningProperties> mining;
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
            .hardness = 0.0f,
            .texture = INT_MAX
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
            .texture = 1,
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 1}}
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
            .texture = 2,
            .mining = MiningProperties{.mining_resistance = 2, .desired_tool = {.tool_type = ToolType::Shovel, .level = 1}}
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
            .texture = 3,
            .mining = MiningProperties{.mining_resistance = 2, .desired_tool = {.tool_type = ToolType::Shovel, .level = 1}}
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
            .texture = 4,
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 1}}
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
            .texture = 5,
            .mining = MiningProperties{.mining_resistance = 50, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 5}}
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
            .texture = 6,
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
            .texture = 11
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
            .texture = 13,
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 2}}
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
            .texture = 14,
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 4}}
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
            .texture = 15,
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Pickaxe, .level = 4}}
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
            .texture = 16,
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Axe, .level = 1}}
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
            .texture = 17,
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Axe, .level = 1}}
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
            .texture = 22,
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Axe, .level = 1}}
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
            .texture = 25,
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
            .texture = 26,
            .mining = MiningProperties{.mining_resistance = 2, .desired_tool = {.tool_type = ToolType::Shovel, .level = 1}}
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
            .texture = 27,
            .mining = MiningProperties{.mining_resistance = 2, .desired_tool = {.tool_type = ToolType::Shovel, .level = 1}}
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
            .texture = 28,
            .mining = MiningProperties{.mining_resistance = 1, .desired_tool = {.tool_type = ToolType::Shovel, .level = 1}}
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