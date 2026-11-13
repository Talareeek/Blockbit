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

// Populated at startup by PackLoader::scanPackRecipes from
// resources/packs/blockbit/recipes/*.json (and any modpack recipes/).
std::unordered_map<ItemStack, Recipe> recipes;
std::unordered_map<ItemID, SmeltRecipe> smeltingRecipes;
std::unordered_map<ItemID, float> fuelBurnTimes;
