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
    Oak_Leaves = 12
};

struct ItemData
{
    std::string name;
    uint32_t texture;
    uint32_t maxStackSize;

    std::function<void(World& world, sf::Vector2f mouse, uint32_t user)> onUse = [](World&, sf::Vector2f, uint32_t){};
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
};

extern bool addItem(Inventory& inventory, ItemID itemID, uint32_t quantity);

#endif // ITEM_HPP