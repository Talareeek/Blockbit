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

// ---- Rubyness ----

bool isRubyTool(ItemID id)
{
    switch (id)
    {
        case ItemID::Ruby_Pickaxe:
        case ItemID::Ruby_Axe:
        case ItemID::Ruby_Shovel:
            return true;
        default:
            return false;
    }
}

uint8_t getRubynessLevel(const ItemStack& stack)
{
    if (!isRubyTool(stack.itemID)) return 0;

    if (stack.instanceID.has_value())
    {
        if (const ItemInstanceData* data = findInstance(*stack.instanceID))
        {
            if (const RubynessItemComponent* r = data->get<RubynessItemComponent>())
            {
                return r->level;
            }
        }
    }
    return RUBYNESS_MIN_LEVEL;
}

void setRubynessLevel(ItemStack& stack, uint8_t level)
{
    if (!isRubyTool(stack.itemID)) return;
    if (level < RUBYNESS_MIN_LEVEL) level = RUBYNESS_MIN_LEVEL;
    if (level > RUBYNESS_MAX_LEVEL) level = RUBYNESS_MAX_LEVEL;

    // Baseline (Alpha) is the implicit default — drop the component so the
    // stack can still merge with other baseline ruby stacks.
    if (level == RUBYNESS_MIN_LEVEL && stack.instanceID.has_value())
    {
        if (ItemInstanceData* data = findInstanceMutable(*stack.instanceID))
        {
            data->remove<RubynessItemComponent>();
            if (data->isEmpty())
            {
                removeInstance(*stack.instanceID);
                stack.instanceID.reset();
            }
        }
        return;
    }

    if (level == RUBYNESS_MIN_LEVEL) return;

    ItemInstanceData& data = ensureInstance(stack);
    data.ensure<RubynessItemComponent>().level = level;
}

float rubynessMultiplier(uint8_t level)
{
    if (level < RUBYNESS_MIN_LEVEL) return 1.0f;
    if (level > RUBYNESS_MAX_LEVEL) level = RUBYNESS_MAX_LEVEL;
    const float span = static_cast<float>(RUBYNESS_MAX_LEVEL - RUBYNESS_MIN_LEVEL);
    return 1.0f + static_cast<float>(level - RUBYNESS_MIN_LEVEL) * (RUBYNESS_OMEGA_MULTIPLIER - 1.0f) / span;
}

std::string greekLetterName(uint8_t level)
{
    static const char* names[RUBYNESS_MAX_LEVEL] = {
        "alpha", "beta", "gamma", "delta", "epsilon", "zeta",
        "eta", "theta", "iota", "kappa", "lambda", "mu",
        "nu", "xi", "omicron", "pi", "rho", "sigma",
        "tau", "upsilon", "phi", "chi", "psi", "omega"
    };
    if (level < RUBYNESS_MIN_LEVEL || level > RUBYNESS_MAX_LEVEL) return {};
    return names[level - 1];
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
