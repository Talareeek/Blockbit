#ifndef BLOCK_HPP
#define BLOCK_HPP

#include <cstdint>
#include <unordered_map>
#include <climits>

enum class BlockID : uint32_t
{
    Air = 0,
    Stone = 1,
    Grass = 2,
    Dirt = 3,
    Cobblestone = 4,
    Obsidian = 5,
    Bedrock = 6
};

struct BlockData
{
    bool solid;
    bool transparent;
    bool breakable;

    float hardness;
    uint32_t texture;
};

inline std::unordered_map<BlockID, BlockData> blockDatabase = 
{
    { BlockID::Air, { false, true, false, 0.0f, INT_MAX}},
    { BlockID::Stone, { true, false, true, 1.5f, 1 }},
    { BlockID::Grass, { true, false, true, 0.6f, 2 }},
    { BlockID::Dirt, { true, false, true, 0.5f, 3 }},
    { BlockID::Cobblestone, { true, false, true, 2.0f, 4 }},
    { BlockID::Obsidian, { true, false, true, 50.0f, 5 }},
    { BlockID::Bedrock, { true, false, false, -1.0f, 6 }}
};

#endif // BLOCK_HPP