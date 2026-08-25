#ifndef BLOCK_HPP
#define BLOCK_HPP

#include <cstdint>
#include <unordered_map>
#include <climits>
#include <optional>
#include "Item.hpp"

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
    uint32_t texture;

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
            .texture = 2,
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
            .texture = 3,
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
            .texture = 4,
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
            .texture = 5,
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
            .texture = 14,
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
            .texture = 15,
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
            .texture = 44,
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
            .texture = 16,
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
            .texture = 17,
            .mining = MiningProperties{.mining_resistance = 5, .desired_tool = {.tool_type = ToolType::Axe, .level = 1}},
            .drop = DropProperties{.drop = ItemID::Oak_Leaves}
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
            .texture = 27,
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
            .texture = 28,
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