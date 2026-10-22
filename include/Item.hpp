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

#include "AssetManager.hpp"

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
    StabShot,
    NukeShot,

    Coal_Ore,
    Iron_Ore,
    Gold_Ore,
    Diamond_Ore,
    Ruby_Ore,

    Iron_Block,
    Gold_Block,
    Ruby_Block,
    Coal_Block,

    Coal,
    Raw_Iron,
    Raw_Gold,
    Diamond,

    Oak_Log,
    Oak_Leaves,
    Oak_Planks,
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
    Ruby_Pickaxe,

    Wooden_Axe,
    Stone_Axe,
    Iron_Axe,
    Gold_Axe,
    Diamond_Axe,
    Ruby_Axe,

    Wooden_Shovel,
    Stone_Shovel,
    Iron_Shovel,
    Gold_Shovel,
    Diamond_Shovel,
    Ruby_Shovel,

    Ruby,
    Stick,

    Chest,
    Bricks,
    Furnace,

    Iron_Ingot,
    Gold_Ingot
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
inline constexpr float RUBY_SPEED = 10.0f;


inline std::unordered_map<int, float> level_based_speed =
{
    {0, HAND_SPEED},
    {1, WOOD_SPEED},
    {2, STONE_SPEED},
    {3, GOLD_SPEED},
    {4, IRON_SPEED},
    {5, DIAMOND_SPEED},
    {6, RUBY_SPEED}
};

struct ToolProperties
{
    ToolType tool_type;

    int level;
};

struct ItemData
{
    std::string name;
    AssetManager::GameTextureID texture;
    uint32_t maxStackSize;

    ItemRarity rarity = ItemRarity::Common;

    ItemCategory category;

    std::optional<ToolProperties> tool;

    std::optional<uint32_t> maxDurability;

    std::function<bool(World& world, sf::Vector2f mouse, UUID user)> onUse = [](World&, sf::Vector2f, UUID) -> bool {return false;};
};

extern std::unordered_map<ItemID, ItemData> itemDatabase;

struct ItemStack
{
    ItemID itemID;
    uint32_t quantity;

    std::optional<UUID> instanceID;

    bool empty() const;

    bool isUnique() const { return instanceID.has_value(); }
};

struct Inventory
{
    std::vector<ItemStack> slots;

    Inventory(size_t size);

    ItemStack addItemWithLeftover(ItemID itemID, uint32_t quantity);

    ItemStack addStackWithLeftover(ItemStack stack);

    ItemStack removeItemWithLeftover(ItemID itemID, uint32_t quantity);
};

extern bool addItem(Inventory& inventory, ItemID itemID, uint32_t quantity);


struct Recipe
{
    std::vector<ItemStack> ingredients;
    bool requires_crafting_table = false;
};

inline bool operator==(const ItemStack& a, const ItemStack& b)
{
    return a.itemID == b.itemID && a.quantity == b.quantity && a.instanceID == b.instanceID;
}

namespace std
{
    template<>
    struct hash<ItemStack>
    {
        size_t operator()(const ItemStack& stack) const noexcept
        {
            size_t h1 = std::hash<uint32_t>{}(static_cast<uint32_t>(stack.itemID));
            size_t h2 = std::hash<uint32_t>{}(stack.quantity);
            size_t h = h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));

            if (stack.instanceID.has_value())
            {
                size_t h3 = std::hash<UUID>{}(*stack.instanceID);
                h ^= h3 + 0x9e3779b9 + (h << 6) + (h >> 2);
            }

            return h;
        }
    };
}

extern std::unordered_map<ItemStack, Recipe> recipes;

extern bool canStack(const ItemStack& a, const ItemStack& b);

// Smelting: input ItemID -> (output ItemID, seconds required).
struct SmeltRecipe
{
    ItemID output;
    float  cook_time;
};

extern std::unordered_map<ItemID, SmeltRecipe> smeltingRecipes;

// Fuel: ItemID -> seconds of burn each unit provides.
extern std::unordered_map<ItemID, float> fuelBurnTimes;

#endif // ITEM_HPP