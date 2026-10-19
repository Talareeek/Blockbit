#include "../include/ItemUtils.hpp"

uint32_t getMaxDurability(ItemID id)
{
    auto it = itemDatabase.find(id);
    if (it == itemDatabase.end()) return 0;
    if (!it->second.maxDurability.has_value()) return 0;
    return *it->second.maxDurability;
}

uint32_t getCurrentDurability(const ItemStack& stack)
{
    uint32_t max_durability = getMaxDurability(stack.itemID);
    if (max_durability == 0) return 0;

    if (!stack.instanceID.has_value()) return max_durability;

    const ItemInstanceData* data = findInstance(*stack.instanceID);
    if (!data || !data->durability.has_value()) return max_durability;

    return *data->durability;
}

std::string getDisplayName(const ItemStack& stack)
{
    if (stack.instanceID.has_value())
    {
        if (const ItemInstanceData* data = findInstance(*stack.instanceID))
        {
            if (data->customName.has_value()) return *data->customName;
        }
    }

    auto it = itemDatabase.find(stack.itemID);
    if (it == itemDatabase.end()) return {};
    return it->second.name;
}

static ItemInstanceData& ensureInstance(ItemStack& stack)
{
    if (!stack.instanceID.has_value())
    {
        stack.instanceID = generateUUID();
    }
    return getOrCreateInstance(*stack.instanceID);
}

bool damageItem(ItemStack& stack, uint32_t amount)
{
    uint32_t max_durability = getMaxDurability(stack.itemID);
    if (max_durability == 0) return true;

    ItemInstanceData& data = ensureInstance(stack);
    if (!data.durability.has_value()) data.durability = max_durability;

    if (amount >= *data.durability)
    {
        removeInstance(*stack.instanceID);
        stack.instanceID.reset();
        stack.itemID = ItemID::None;
        stack.quantity = 0;
        return false;
    }

    *data.durability -= amount;
    return true;
}

void setCustomName(ItemStack& stack, const std::string& name)
{
    ItemInstanceData& data = ensureInstance(stack);
    data.customName = name;
}

void addEnchantment(ItemStack& stack, Enchantment enchantment)
{
    ItemInstanceData& data = ensureInstance(stack);

    for (Enchantment& existing : data.enchantments)
    {
        if (existing.id == enchantment.id)
        {
            existing.level = enchantment.level;
            return;
        }
    }

    data.enchantments.push_back(enchantment);
}
