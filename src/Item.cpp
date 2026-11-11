#include "../include/Item.hpp"
#include "../include/World.hpp"

#include <cctype>
#include "../include/Entity.hpp"
#include "../include/ExplosiveComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/Chunk.hpp"
#include "../include/FurnaceBlockEntity.hpp"
#include "../include/EffectsComponent.hpp"
#include "../include/ItemInstance.hpp"
#include "../include/ItemDataComponents.hpp"


// itemDatabase is now populated by PackLoader at startup from
// resources/packs/blockbit/items/*.json. The old static initializer
// was removed in Phase 1c-E — see registerVanillaContent().
std::unordered_map<ItemID, ItemData> itemDatabase;

bool ItemStack::empty() const
{
    return itemID == ItemID::None || quantity == 0;
}

bool canStack(const ItemStack& a, const ItemStack& b)
{
    if (a.instanceID.has_value() || b.instanceID.has_value()) return false;
    return a.itemID == b.itemID;
}

Inventory::Inventory(size_t size)
{
    slots.resize(size, {ItemID::None, 0});
}

std::optional<ItemID> findItemIdByName(const std::string& name)
{
    auto normalize = [](const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s)
        {
            if (c == '_') c = ' ';
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return out;
    };

    const std::string needle = normalize(name);

    for (const auto& [id, data] : itemDatabase)
    {
        if (normalize(data.name) == needle) return id;
    }
    return std::nullopt;
}

bool addItem(Inventory& inventory, ItemID itemID, uint32_t quantity)
{
    auto& data = itemDatabase[itemID];

    for(auto& slot : inventory.slots)
    {
        if(slot.itemID == itemID && !slot.instanceID.has_value() && slot.quantity < data.maxStackSize)
        {
            uint32_t space = data.maxStackSize - slot.quantity;
            uint32_t add = std::min(space, quantity);

            slot.quantity += add;
            quantity -= add;

            if(quantity == 0)
                return true;
        }
    }

    for(auto& slot : inventory.slots)
    {
        if(slot.empty())
        {
            slot.itemID = itemID;
            slot.quantity = std::min(quantity, data.maxStackSize);

            quantity -= slot.quantity;

            if(quantity == 0)
                return true;
        }
    }

    return false;
}

ItemStack Inventory::addItemWithLeftover(ItemID itemID, uint32_t quantity)
{
    auto& data = itemDatabase[itemID];

    for(auto& slot : slots)
    {
        if(slot.itemID == itemID && !slot.instanceID.has_value() && slot.quantity < data.maxStackSize)
        {
            uint32_t space = data.maxStackSize - slot.quantity;
            uint32_t add = std::min(space, quantity);

            slot.quantity += add;
            quantity -= add;

            if(quantity == 0)
                return {ItemID::None, 0};
        }
    }

    for(auto& slot : slots)
    {
        if(slot.empty())
        {
            slot.itemID = itemID;
            slot.quantity = std::min(quantity, data.maxStackSize);

            quantity -= slot.quantity;

            if(quantity == 0)
                return {ItemID::None, 0};
        }
    }

    return {itemID, quantity};
}

ItemStack Inventory::addStackWithLeftover(ItemStack stack)
{
    if (stack.empty()) return {ItemID::None, 0};

    if (stack.instanceID.has_value())
    {
        for (auto& slot : slots)
        {
            if (slot.empty())
            {
                slot = stack;
                return {ItemID::None, 0};
            }
        }
        return stack;
    }

    ItemStack leftover = addItemWithLeftover(stack.itemID, stack.quantity);
    return leftover;
}

ItemStack Inventory::removeItemWithLeftover(ItemID itemID, uint32_t quantity)
{
    for(auto& slot : slots)
    {
        if(quantity == 0) return {ItemID::None, 0};

        if(slot.itemID == itemID && !slot.instanceID.has_value())
        {
            uint32_t remove = std::min(slot.quantity, quantity);

            slot.quantity -= remove;
            quantity -= remove;

            if(slot.quantity == 0) slot.itemID = ItemID::None;
        }
    }

    if(quantity == 0) return {ItemID::None, 0};

    return {itemID, quantity};
}

std::unordered_map<ItemStack, Recipe> recipes
{
    {{.itemID = ItemID::Oak_Planks, .quantity = 4}, {{{ItemID::Oak_Log, 1}}, false}},
    {{.itemID = ItemID::Stick, .quantity = 4}, {{{ItemID::Oak_Planks, 2}}, false}},
    {{.itemID = ItemID::Workbench, .quantity = 1}, {{{ItemID::Oak_Planks, 4}}, false}},
    {{.itemID = ItemID::Furnace,   .quantity = 1}, {{{ItemID::Cobblestone, 8}}, true}},

    {{.itemID = ItemID::Wooden_Pickaxe,  .quantity = 1}, {{{ItemID::Oak_Planks, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Stone_Pickaxe,   .quantity = 1}, {{{ItemID::Cobblestone, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Iron_Pickaxe,    .quantity = 1}, {{{ItemID::Iron_Ingot, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Gold_Pickaxe,    .quantity = 1}, {{{ItemID::Gold_Ingot, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Diamond_Pickaxe, .quantity = 1}, {{{ItemID::Diamond, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Ruby_Pickaxe,    .quantity = 1}, {{{ItemID::Ruby, 3}, {ItemID::Stick, 2}}, true}},

    {{.itemID = ItemID::Wooden_Axe,  .quantity = 1}, {{{ItemID::Oak_Planks, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Stone_Axe,   .quantity = 1}, {{{ItemID::Cobblestone, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Iron_Axe,    .quantity = 1}, {{{ItemID::Iron_Ingot, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Gold_Axe,    .quantity = 1}, {{{ItemID::Gold_Ingot, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Diamond_Axe, .quantity = 1}, {{{ItemID::Diamond, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Ruby_Axe,    .quantity = 1}, {{{ItemID::Ruby, 3}, {ItemID::Stick, 2}}, true}},

    {{.itemID = ItemID::Wooden_Shovel,  .quantity = 1}, {{{ItemID::Oak_Planks, 1}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Stone_Shovel,   .quantity = 1}, {{{ItemID::Cobblestone, 1}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Iron_Shovel,    .quantity = 1}, {{{ItemID::Iron_Ingot, 1}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Gold_Shovel,    .quantity = 1}, {{{ItemID::Gold_Ingot, 1}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Diamond_Shovel, .quantity = 1}, {{{ItemID::Diamond, 1}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Ruby_Shovel,    .quantity = 1}, {{{ItemID::Ruby, 1}, {ItemID::Stick, 2}}, true}},

    {{.itemID = ItemID::Iron_Block, .quantity = 1}, {{{ItemID::Iron_Ingot, 9}}, true}},
    {{.itemID = ItemID::Gold_Block, .quantity = 1}, {{{ItemID::Gold_Ingot, 9}}, true}},
    {{.itemID = ItemID::Ruby_Block, .quantity = 1}, {{{ItemID::Ruby, 9}}, true}},
    {{.itemID = ItemID::Coal_Block, .quantity = 1}, {{{ItemID::Coal, 9}}, true}},

    {{.itemID = ItemID::Iron_Ingot, .quantity = 9}, {{{ItemID::Iron_Block, 1}}, true}},
    {{.itemID = ItemID::Gold_Ingot, .quantity = 9}, {{{ItemID::Gold_Block, 1}}, true}},
    {{.itemID = ItemID::Ruby,       .quantity = 9}, {{{ItemID::Ruby_Block, 1}}, true}},
    {{.itemID = ItemID::Coal,       .quantity = 9}, {{{ItemID::Coal_Block, 1}}, true}},
};

std::unordered_map<ItemID, SmeltRecipe> smeltingRecipes
{
    {ItemID::Cobblestone, {ItemID::Stone,      10.0f}},
    {ItemID::Raw_Iron,    {ItemID::Iron_Ingot, 10.0f}},
    {ItemID::Raw_Gold,    {ItemID::Gold_Ingot, 10.0f}},
};

std::unordered_map<ItemID, float> fuelBurnTimes
{
    {ItemID::Coal,       80.0f},
    {ItemID::Oak_Log,       15.0f},
    {ItemID::Oak_Planks,    15.0f},
    {ItemID::Oak_Leaves,     1.0f},
    {ItemID::Birch_Log,     15.0f},
    {ItemID::Birch_Leaves,   1.0f},
    {ItemID::Spruce_Log,    15.0f},
    {ItemID::Spruce_Leaves,  1.0f},
    {ItemID::Woodcutter, 15.0f},
    {ItemID::Stick,       5.0f},
};
