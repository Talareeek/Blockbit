#ifndef BLOCK_HPP
#define BLOCK_HPP

#include <cstdint>
#include <unordered_map>
#include <climits>
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
    Oak_Leaves = 12
};

struct BlockData
{
    bool solid;
    bool transparent;
    bool breakable;

    float hardness;
    uint32_t texture;
};

struct Block
{
    BlockID id;
    uint8_t metadata;
};

inline std::unordered_map<BlockID, BlockData> blockDatabase = 
{
    { BlockID::Air, { false, true, false, 0.0f, INT_MAX}},
    { BlockID::Stone, { true, false, true, 1.5f, 1 }},
    { BlockID::Grass, { true, false, true, 0.6f, 2 }},
    { BlockID::Dirt, { true, false, true, 0.5f, 3 }},
    { BlockID::Cobblestone, { true, false, true, 2.0f, 4 }},
    { BlockID::Obsidian, { true, false, true, 50.0f, 5 }},
    { BlockID::Bedrock, { true, false, false, -1.0f, 6 }},
    { BlockID::Water, { false, true, false, 0.0f, 11 }},
    { BlockID::Iron_Ore, { true, false, true, 3.0f, 13 }},
    { BlockID::Gold_Ore, { true, false, true, 3.0f, 14 }},
    { BlockID::Diamond_Ore, { true, false, true, 5.0f, 15 }},
    { BlockID::Oak_Log, { true, false, true, 2.0f, 16 }},
    { BlockID::Oak_Leaves, { true, true, true, 0.2f, 17 }}
};


enum class WaterLevel : uint8_t
{
    FULL = 8,
    SOURCE = 9
};

extern ItemID blockToItem(BlockID block);

extern BlockID itemToBlock(ItemID item);

#endif // BLOCK_HPP