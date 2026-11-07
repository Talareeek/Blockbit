#include "../include/ItemUtils.hpp"

#include "../include/ItemDataComponents.hpp"

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
    if (!data) return max_durability;

    const DamageItemComponent* dmg = data->get<DamageItemComponent>();
    if (!dmg) return max_durability;

    return (dmg->damage >= max_durability) ? 0 : (max_durability - dmg->damage);
}

std::string getDisplayName(const ItemStack& stack)
{
    if (stack.instanceID.has_value())
    {
        if (const ItemInstanceData* data = findInstance(*stack.instanceID))
        {
            if (const CustomNameItemComponent* name = data->get<CustomNameItemComponent>())
            {
                if (!name->name.empty()) return name->name;
            }
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
    DamageItemComponent& dmg = data.ensure<DamageItemComponent>();

    uint64_t new_damage = static_cast<uint64_t>(dmg.damage) + amount;

    if (new_damage >= max_durability)
    {
        removeInstance(*stack.instanceID);
        stack.instanceID.reset();
        stack.itemID = ItemID::None;
        stack.quantity = 0;
        return false;
    }

    dmg.damage = static_cast<uint32_t>(new_damage);
    return true;
}

void setCustomName(ItemStack& stack, const std::string& name)
{
    if (name.empty() && stack.instanceID.has_value())
    {
        if (ItemInstanceData* data = findInstanceMutable(*stack.instanceID))
        {
            data->remove<CustomNameItemComponent>();
            if (data->isEmpty())
            {
                removeInstance(*stack.instanceID);
                stack.instanceID.reset();
            }
        }
        return;
    }

    ItemInstanceData& data = ensureInstance(stack);
    data.ensure<CustomNameItemComponent>().name = name;
}

void addEnchantment(ItemStack& stack, Enchantment enchantment)
{
    ItemInstanceData& data = ensureInstance(stack);
    EnchantmentsItemComponent& ench = data.ensure<EnchantmentsItemComponent>();

    for (Enchantment& existing : ench.entries)
    {
        if (existing.id == enchantment.id)
        {
            existing.level = enchantment.level;
            return;
        }
    }

    ench.entries.push_back(enchantment);
}

// ---- Stack serialization ----

Tag stackToTag(const ItemStack& stack)
{
    TagCompound compound;

    compound["item_id"]  = Tag(static_cast<uint32_t>(stack.itemID));
    compound["quantity"] = Tag(stack.quantity);

    if (stack.instanceID.has_value())
    {
        compound["instance_id"] = Tag(stack.instanceID->toString());

        if (const ItemInstanceData* data = findInstance(*stack.instanceID))
        {
            compound["instance_data"] = data->serialize();
        }
    }

    return Tag(compound);
}

ItemStack stackFromTag(const Tag& tag, bool write_instance_data)
{
    ItemStack out{ItemID::None, 0};

    const auto& compound = tag.get<TagCompound>();

    if (auto it = compound.find("item_id"); it != compound.end())
    {
        out.itemID = static_cast<ItemID>(it->second.get<uint32_t>());
    }

    if (auto it = compound.find("quantity"); it != compound.end())
    {
        out.quantity = it->second.get<uint32_t>();
    }

    out.instanceID.reset();

    if (auto it = compound.find("instance_id"); it != compound.end())
    {
        out.instanceID = uuidFromString(it->second.get<std::string>());

        if (write_instance_data && out.instanceID.has_value())
        {
            if (auto data_it = compound.find("instance_data"); data_it != compound.end())
            {
                itemInstances[*out.instanceID] = ItemInstanceData::deserialize(data_it->second);
            }
        }
    }

    return out;
}

std::vector<uint8_t> stackToBytes(const ItemStack& stack)
{
    BBT bbt("stack");
    bbt.root() = stackToTag(stack).get<TagCompound>();
    return bbt.save();
}

std::optional<ItemStack> stackFromBytes(const std::vector<uint8_t>& buffer, bool write_instance_data)
{
    if (buffer.empty()) return std::nullopt;

    try
    {
        BBT bbt = BBT::load(buffer);
        return stackFromTag(Tag(bbt.root()), write_instance_data);
    }
    catch (...)
    {
        return std::nullopt;
    }
}
