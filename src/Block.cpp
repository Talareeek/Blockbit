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
        case BlockID::Oak_Log: return ItemID::Oak_Log;
        case BlockID::Oak_Leaves: return ItemID::Oak_Leaves;
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
        case ItemID::Oak_Log: return BlockID::Oak_Log;
        case ItemID::Oak_Leaves: return BlockID::Oak_Leaves;
        default: return BlockID::Air;
    }
}