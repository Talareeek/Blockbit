#ifndef ITEM_HPP
#define ITEM_HPP

#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>

#include <SFML/System/Vector2.hpp>

#include "../include/Entity.hpp"

class World;

enum class ItemID : uint32_t
{
    None = 0,
    Stone = 1,
    Grass = 2,
    Dirt = 3,
    Cobblestone = 4,
    Obsidian = 5,
    Bedrock = 6,
    Dynamite = 7,
    Iron_Ore = 8,
    Gold_Ore = 9,
    Diamond_Ore = 10,
    Oak_Log = 11,
    Oak_Leaves = 12,
    Bucket = 13,
    Water_Bucket = 14,
    Woodcutter = 15
};

enum class ItemRarity
{
    Common,
    Rare,
    Super_Rare,
    Epic,
    Mythic
};

enum class ItemCategory
{
    Block,
    Tool,
    Weapon,
    Consumable,
    Misc
};

enum class RecyclingCategory
{
    None,
    Wood,
    Stone,
    Organic,
    Ore
};

struct CraftMaterials
{
    uint32_t wood;
    uint32_t stone;
    uint32_t iron;
    uint32_t gold;
    uint32_t diamond;
};


struct ItemData
{
    std::string name;
    uint32_t texture;
    uint32_t maxStackSize;

    ItemRarity rarity;

    ItemCategory category;

    bool recycleable;

    std::function<bool(World& world, sf::Vector2f mouse, uint32_t user)> onUse = [](World&, sf::Vector2f, uint32_t) -> bool {return false;};    

    //RecyclingCategory recycling_category = RecyclingCategory::None;

    //CraftMaterials materials = {0, 0, 0, 0, 0};
};

extern std::unordered_map<ItemID, ItemData> itemDatabase;


struct ItemStack
{
    ItemID itemID;
    uint32_t quantity;

    bool empty() const;
};

struct Inventory
{
    std::vector<ItemStack> slots;

    Inventory(size_t size);

    ItemStack addItemWithLeftover(ItemID itemID, uint32_t quantity);

    ItemStack removeItemWithLeftover(ItemID itemID, uint32_t quantity);
};

extern bool addItem(Inventory& inventory, ItemID itemID, uint32_t quantity);

#endif // ITEM_HPP