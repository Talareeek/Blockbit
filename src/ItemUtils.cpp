#include "../include/ItemUtils.hpp"

#include "../include/ItemDataComponents.hpp"

ItemKey itemKeyFromLegacySave(uint32_t old_id)
{
    static const std::vector<ItemKey> old_item_order = {Items::None, Items::Stone, Items::Grass, Items::Dirt, Items::Cobblestone, Items::Obsidian, Items::Bedrock, Items::Dynamite, Items::StabShot, Items::NukeShot, Items::Coal_Ore, Items::Iron_Ore, Items::Gold_Ore, Items::Diamond_Ore, Items::Ruby_Ore, Items::Iron_Block, Items::Gold_Block, Items::Ruby_Block, Items::Coal_Block, Items::Coal, Items::Raw_Iron, Items::Raw_Gold, Items::Diamond, Items::Oak_Log, Items::Oak_Leaves, Items::Oak_Planks, Items::Birch_Log, Items::Birch_Leaves, Items::Spruce_Log, Items::Spruce_Leaves, Items::Bucket, Items::Water_Bucket, Items::Woodcutter, Items::Lighter, Items::Sand, Items::Coarse_Dirt, Items::Snow, Items::Gravel, Items::Flint, Items::Wooden_Pickaxe, Items::Stone_Pickaxe, Items::Iron_Pickaxe, Items::Gold_Pickaxe, Items::Diamond_Pickaxe, Items::Ruby_Pickaxe, Items::Wooden_Axe, Items::Stone_Axe, Items::Iron_Axe, Items::Gold_Axe, Items::Diamond_Axe, Items::Ruby_Axe, Items::Wooden_Shovel, Items::Stone_Shovel, Items::Iron_Shovel, Items::Gold_Shovel, Items::Diamond_Shovel, Items::Ruby_Shovel, Items::Ruby, Items::Stick, Items::Chest, Items::Bricks, Items::Furnace, Items::Iron_Ingot, Items::Gold_Ingot, Items::Workbench, Items::Cactus, Items::Dead_Bush, Items::Smooth_Stone, Items::Diorite, Items::Granite, Items::Glass, Items::Output_Bulb, Items::Ruby_Wire, Items::Coreheart_Of_Blockbit, Items::Upgrade_Table, Items::Magic_Bottle, Items::Birch_Planks, Items::Spruce_Planks, Items::Copper_Ore, Items::Copper_Ingot, Items::Copper_Block};
    return old_id < old_item_order.size() ? old_item_order[old_id] : Items::None;
}


uint32_t getMaxDurability(ItemKey id)
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
        stack.itemID = Items::None;
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

bool isRubyTool(ItemKey id)
{
    return id == Items::Ruby_Pickaxe || id == Items::Ruby_Axe || id == Items::Ruby_Shovel;
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

    compound["item_id"]  = Tag(stack.itemID);
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
    ItemStack out{Items::None, 0};

    const auto& compound = tag.get<TagCompound>();

    if (auto it = compound.find("item_id"); it != compound.end())
    {
        if (it->second.type() == TagType::T_STRING)
            out.itemID = it->second.get<std::string>();
        else if (it->second.type() == TagType::T_UINT32)
            out.itemID = itemKeyFromLegacySave(it->second.get<uint32_t>());
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
