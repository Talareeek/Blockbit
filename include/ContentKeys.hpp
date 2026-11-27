#ifndef CONTENT_KEYS_HPP
#define CONTENT_KEYS_HPP

#include <string>
#include <vector>
#include <cstdint>

// Canonical names replace legacy numeric block and item IDs.
using BlockKey = std::string;
using ItemKey = std::string;

namespace Blocks
{
inline const BlockKey Air = "blockbit:air";
inline const BlockKey Stone = "blockbit:stone";
inline const BlockKey Grass = "blockbit:grass_block";
inline const BlockKey Dirt = "blockbit:dirt";
inline const BlockKey Cobblestone = "blockbit:cobblestone";
inline const BlockKey Obsidian = "blockbit:obsidian";
inline const BlockKey Bedrock = "blockbit:bedrock";
inline const BlockKey Water = "blockbit:water";
inline const BlockKey Lava = "blockbit:lava";
inline const BlockKey Coal_Ore = "blockbit:coal_ore";
inline const BlockKey Iron_Ore = "blockbit:iron_ore";
inline const BlockKey Gold_Ore = "blockbit:gold_ore";
inline const BlockKey Diamond_Ore = "blockbit:diamond_ore";
inline const BlockKey Ruby_Ore = "blockbit:ruby_ore";
inline const BlockKey Iron_Block = "blockbit:iron_block";
inline const BlockKey Gold_Block = "blockbit:gold_block";
inline const BlockKey Ruby_Block = "blockbit:ruby_block";
inline const BlockKey Coal_Block = "blockbit:coal_block";
inline const BlockKey Oak_Log = "blockbit:oak_log";
inline const BlockKey Oak_Leaves = "blockbit:oak_leaves";
inline const BlockKey Oak_Planks = "blockbit:oak_planks";
inline const BlockKey Birch_Log = "blockbit:birch_log";
inline const BlockKey Birch_Leaves = "blockbit:birch_leaves";
inline const BlockKey Spruce_Log = "blockbit:spruce_log";
inline const BlockKey Spruce_Leaves = "blockbit:spruce_leaves";
inline const BlockKey Woodcutter = "blockbit:woodcutter";
inline const BlockKey Fire = "blockbit:fire";
inline const BlockKey Sand = "blockbit:sand";
inline const BlockKey Coarse_Dirt = "blockbit:coarse_dirt";
inline const BlockKey Snow = "blockbit:snow";
inline const BlockKey Chest = "blockbit:chest";
inline const BlockKey Bricks = "blockbit:bricks";
inline const BlockKey Furnace = "blockbit:furnace";
inline const BlockKey Workbench = "blockbit:workbench";
inline const BlockKey Gravel = "blockbit:gravel";
inline const BlockKey Cactus = "blockbit:cactus";
inline const BlockKey Dead_Bush = "blockbit:dead_bush";
inline const BlockKey Tall_Grass = "blockbit:grass";
inline const BlockKey Dandelion = "blockbit:dandelion";
inline const BlockKey Poppy = "blockbit:poppy";
inline const BlockKey Smooth_Stone = "blockbit:smooth_stone";
inline const BlockKey Diorite = "blockbit:diorite";
inline const BlockKey Granite = "blockbit:granite";
inline const BlockKey Glass = "blockbit:glass";
inline const BlockKey Output_Bulb = "blockbit:output_bulb";
inline const BlockKey Ruby_Wire = "blockbit:ruby_wire";
inline const BlockKey Coreheart_Of_Blockbit = "blockbit:coreheart_of_blockbit";
inline const BlockKey Upgrade_Table = "blockbit:upgrade_table";
inline const BlockKey Birch_Planks = "blockbit:birch_planks";
inline const BlockKey Spruce_Planks = "blockbit:spruce_planks";
inline const BlockKey Copper_Ore = "blockbit:copper_ore";
inline const BlockKey Copper_Block = "blockbit:copper_block";
}

namespace Items
{
inline const ItemKey Stone = "blockbit:stone";
inline const ItemKey Grass = "blockbit:grass_block";
inline const ItemKey Dirt = "blockbit:dirt";
inline const ItemKey Cobblestone = "blockbit:cobblestone";
inline const ItemKey Obsidian = "blockbit:obsidian";
inline const ItemKey Bedrock = "blockbit:bedrock";
inline const ItemKey Dynamite = "blockbit:dynamite";
inline const ItemKey StabShot = "blockbit:stabshot";
inline const ItemKey NukeShot = "blockbit:nukeshot";
inline const ItemKey Coal_Ore = "blockbit:coal_ore";
inline const ItemKey Iron_Ore = "blockbit:iron_ore";
inline const ItemKey Gold_Ore = "blockbit:gold_ore";
inline const ItemKey Diamond_Ore = "blockbit:diamond_ore";
inline const ItemKey Ruby_Ore = "blockbit:ruby_ore";
inline const ItemKey Iron_Block = "blockbit:iron_block";
inline const ItemKey Gold_Block = "blockbit:gold_block";
inline const ItemKey Ruby_Block = "blockbit:ruby_block";
inline const ItemKey Coal_Block = "blockbit:coal_block";
inline const ItemKey Coal = "blockbit:coal";
inline const ItemKey Raw_Iron = "blockbit:raw_iron";
inline const ItemKey Raw_Gold = "blockbit:raw_gold";
inline const ItemKey Diamond = "blockbit:diamond";
inline const ItemKey Oak_Log = "blockbit:oak_log";
inline const ItemKey Oak_Leaves = "blockbit:oak_leaves";
inline const ItemKey Oak_Planks = "blockbit:oak_planks";
inline const ItemKey Birch_Log = "blockbit:birch_log";
inline const ItemKey Birch_Leaves = "blockbit:birch_leaves";
inline const ItemKey Spruce_Log = "blockbit:spruce_log";
inline const ItemKey Spruce_Leaves = "blockbit:spruce_leaves";
inline const ItemKey Bucket = "blockbit:bucket";
inline const ItemKey Water_Bucket = "blockbit:water_bucket";
inline const ItemKey Woodcutter = "blockbit:woodcutter";
inline const ItemKey Lighter = "blockbit:lighter";
inline const ItemKey Sand = "blockbit:sand";
inline const ItemKey Coarse_Dirt = "blockbit:coarse_dirt";
inline const ItemKey Snow = "blockbit:snow";
inline const ItemKey Gravel = "blockbit:gravel";
inline const ItemKey Flint = "blockbit:flint";
inline const ItemKey Wooden_Pickaxe = "blockbit:wooden_pickaxe";
inline const ItemKey Stone_Pickaxe = "blockbit:stone_pickaxe";
inline const ItemKey Iron_Pickaxe = "blockbit:iron_pickaxe";
inline const ItemKey Gold_Pickaxe = "blockbit:gold_pickaxe";
inline const ItemKey Diamond_Pickaxe = "blockbit:diamond_pickaxe";
inline const ItemKey Ruby_Pickaxe = "blockbit:ruby_pickaxe";
inline const ItemKey Wooden_Axe = "blockbit:wooden_axe";
inline const ItemKey Stone_Axe = "blockbit:stone_axe";
inline const ItemKey Iron_Axe = "blockbit:iron_axe";
inline const ItemKey Gold_Axe = "blockbit:gold_axe";
inline const ItemKey Diamond_Axe = "blockbit:diamond_axe";
inline const ItemKey Ruby_Axe = "blockbit:ruby_axe";
inline const ItemKey Wooden_Shovel = "blockbit:wooden_shovel";
inline const ItemKey Stone_Shovel = "blockbit:stone_shovel";
inline const ItemKey Iron_Shovel = "blockbit:iron_shovel";
inline const ItemKey Gold_Shovel = "blockbit:gold_shovel";
inline const ItemKey Diamond_Shovel = "blockbit:diamond_shovel";
inline const ItemKey Ruby_Shovel = "blockbit:ruby_shovel";
inline const ItemKey Ruby = "blockbit:ruby";
inline const ItemKey Stick = "blockbit:stick";
inline const ItemKey Chest = "blockbit:chest";
inline const ItemKey Bricks = "blockbit:bricks";
inline const ItemKey Furnace = "blockbit:furnace";
inline const ItemKey Iron_Ingot = "blockbit:iron_ingot";
inline const ItemKey Gold_Ingot = "blockbit:gold_ingot";
inline const ItemKey Workbench = "blockbit:workbench";
inline const ItemKey Cactus = "blockbit:cactus";
inline const ItemKey Dead_Bush = "blockbit:dead_bush";
inline const ItemKey Tall_Grass = "blockbit:grass";
inline const ItemKey Dandelion = "blockbit:dandelion";
inline const ItemKey Poppy = "blockbit:poppy";
inline const ItemKey Smooth_Stone = "blockbit:smooth_stone";
inline const ItemKey Diorite = "blockbit:diorite";
inline const ItemKey Granite = "blockbit:granite";
inline const ItemKey Glass = "blockbit:glass";
inline const ItemKey Output_Bulb = "blockbit:output_bulb";
inline const ItemKey Ruby_Wire = "blockbit:ruby_wire";
inline const ItemKey Coreheart_Of_Blockbit = "blockbit:coreheart_of_blockbit";
inline const ItemKey Upgrade_Table = "blockbit:upgrade_table";
inline const ItemKey Magic_Bottle = "blockbit:magic_bottle";
inline const ItemKey Birch_Planks = "blockbit:birch_planks";
inline const ItemKey Spruce_Planks = "blockbit:spruce_planks";
inline const ItemKey Copper_Ore = "blockbit:copper_ore";
inline const ItemKey Copper_Ingot = "blockbit:copper_ingot";
inline const ItemKey Copper_Block = "blockbit:copper_block";
inline const ItemKey None = "blockbit:air";
}

// One-way compatibility for old saves and structure templates. New writes
// always persist the full key and never write these historic numeric values.
inline BlockKey blockKeyFromLegacySave(uint32_t old_id)
{
    static const std::vector<BlockKey> old_block_order = {
        Blocks::Air, Blocks::Stone, Blocks::Grass, Blocks::Dirt,
        Blocks::Cobblestone, Blocks::Obsidian, Blocks::Bedrock, Blocks::Water,
        Blocks::Lava, Blocks::Coal_Ore, Blocks::Iron_Ore, Blocks::Gold_Ore,
        Blocks::Diamond_Ore, Blocks::Ruby_Ore, Blocks::Iron_Block, Blocks::Gold_Block,
        Blocks::Ruby_Block, Blocks::Coal_Block, Blocks::Oak_Log, Blocks::Oak_Leaves,
        Blocks::Oak_Planks, Blocks::Birch_Log, Blocks::Birch_Leaves, Blocks::Spruce_Log,
        Blocks::Spruce_Leaves, Blocks::Woodcutter, Blocks::Fire, Blocks::Sand,
        Blocks::Coarse_Dirt, Blocks::Snow, Blocks::Chest, Blocks::Bricks,
        Blocks::Furnace, Blocks::Workbench, Blocks::Gravel, Blocks::Cactus,
        Blocks::Dead_Bush, Blocks::Smooth_Stone, Blocks::Diorite, Blocks::Granite,
        Blocks::Glass, Blocks::Output_Bulb, Blocks::Ruby_Wire,
        Blocks::Coreheart_Of_Blockbit, Blocks::Upgrade_Table, Blocks::Birch_Planks,
        Blocks::Spruce_Planks, Blocks::Copper_Ore, Blocks::Copper_Block
    };
    return old_id < old_block_order.size() ? old_block_order[old_id] : Blocks::Air;
}

#endif // CONTENT_KEYS_HPP
