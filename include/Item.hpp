#ifndef ITEM_HPP
#define ITEM_HPP

#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <optional>

#include "UUID.hpp"

#include <SFML/System/Vector2.hpp>

#include "../include/Entity.hpp"

class World;

enum class ItemID : uint32_t
{
    None = 0,
    Stone,
    Grass,
    Dirt,
    Cobblestone,
    Obsidian,
    Bedrock,
    Dynamite,

    Iron_Ore,
    Gold_Ore,
    Diamond_Ore,
    Ruby_Ore,

    Oak_Log,
    Oak_Leaves,
    Bucket,
    Water_Bucket,
    Woodcutter,
    Lighter,
    Sand,
    Coarse_Dirt,
    Snow,

    // TOOLS
    Wooden_Pickaxe,
    Stone_Pickaxe,
    Iron_Pickaxe,
    Gold_Pickaxe,
    Diamond_Pickaxe,

    Wooden_Axe,
    Stone_Axe,
    Iron_Axe,
    Gold_Axe,
    Diamond_Axe,

    Wooden_Shovel,
    Stone_Shovel,
    Iron_Shovel,
    Gold_Shovel,
    Diamond_Shovel,

    Ruby
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

enum class ToolType
{
    Pickaxe,
    Axe,
    Shovel,
    None
};

inline constexpr float HAND_SPEED = 1.0f;
inline constexpr float WOOD_SPEED = 2.0f;
inline constexpr float STONE_SPEED = 4.0f;
inline constexpr float GOLD_SPEED = 12.0f;
inline constexpr float IRON_SPEED = 6.0f;
inline constexpr float DIAMOND_SPEED = 8.0f;


inline std::unordered_map<int, float> level_based_speed =
{
    {0, HAND_SPEED},
    {1, WOOD_SPEED},
    {2, STONE_SPEED},
    {3, GOLD_SPEED},
    {4, IRON_SPEED},
    {5, DIAMOND_SPEED}
};

struct ToolProperties
{
    ToolType tool_type;

    int level;
};

struct ItemData
{
    std::string name;
    uint32_t texture;
    uint32_t maxStackSize;

    ItemRarity rarity = ItemRarity::Common;

    ItemCategory category;

    std::optional<ToolProperties> tool;

    std::function<bool(World& world, sf::Vector2f mouse, UUID user)> onUse = [](World&, sf::Vector2f, UUID) -> bool {return false;};    
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