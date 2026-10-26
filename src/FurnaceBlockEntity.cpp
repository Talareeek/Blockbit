#include "../include/FurnaceBlockEntity.hpp"
#include "../include/World.hpp"
#include "../include/Block.hpp"

namespace
{
    bool canAcceptOutput(const ItemStack& output, ItemID smelted)
    {
        if (output.empty()) return true;
        if (output.itemID != smelted) return false;
        if (output.instanceID.has_value()) return false;
        return output.quantity < itemDatabase[smelted].maxStackSize;
    }

    bool canSmelt(const FurnaceBlockEntity& furnace)
    {
        if (furnace.input.empty()) return false;
        auto it = smeltingRecipes.find(furnace.input.itemID);
        if (it == smeltingRecipes.end()) return false;
        return canAcceptOutput(furnace.output, it->second.output);
    }
}

FurnaceBlockEntity::FurnaceBlockEntity(sf::Vector2i position) : BlockEntity(position)
{
}

void FurnaceBlockEntity::tick(World& world, float dt)
{
    const bool was_lit = lit;

    if (!lit)
    {
        // Pre-ignition: don't advance anything. The Lighter is the only path
        // that turns `lit` on.
        burn_time_remaining = 0.0f;
        burn_time_total     = 0.0f;
        cook_time_remaining = 0.0f;
        cook_time_total     = 0.0f;
    }
    else
    {
        if (burn_time_remaining > 0.0f)
        {
            burn_time_remaining -= dt;
            if (burn_time_remaining < 0.0f) burn_time_remaining = 0.0f;
        }

        if (burn_time_remaining > 0.0f && canSmelt(*this))
        {
            const SmeltRecipe& recipe = smeltingRecipes[input.itemID];

            if (cook_time_total <= 0.0f)
            {
                cook_time_total     = recipe.cook_time;
                cook_time_remaining = recipe.cook_time;
            }

            cook_time_remaining -= dt;

            if (cook_time_remaining <= 0.0f)
            {
                if (output.empty())
                {
                    output = {recipe.output, 1};
                }
                else
                {
                    output.quantity += 1;
                }

                if (--input.quantity == 0) input = {ItemID::None, 0};

                cook_time_total     = 0.0f;
                cook_time_remaining = 0.0f;
            }
        }
        else
        {
            // Nothing valid to smelt — reset cook progress so a freshly placed
            // input starts from zero.
            cook_time_total     = 0.0f;
            cook_time_remaining = 0.0f;
        }

        if (burn_time_remaining <= 0.0f)
        {
            auto fuel_it = fuelBurnTimes.find(fuel.itemID);
            if (canSmelt(*this) && !fuel.empty() && fuel_it != fuelBurnTimes.end())
            {
                burn_time_total     = fuel_it->second;
                burn_time_remaining = fuel_it->second;

                if (--fuel.quantity == 0) fuel = {ItemID::None, 0};
            }
            else
            {
                // Out of fuel or nothing left to smelt — extinguish.
                lit = false;
                burn_time_total = 0.0f;
            }
        }
    }

    const bool now_lit = lit;

    if (was_lit != now_lit)
    {
        Block current = world.getBlock(this->position.x, this->position.y);
        if (current.id == BlockID::Furnace)
        {
            const uint16_t desired_meta = FurnaceState{now_lit}.pack();
            if (current.metadata != desired_meta)
            {
                world.setBlock(this->position.x, this->position.y, {BlockID::Furnace, desired_meta});
            }
        }
    }
}

Tag FurnaceBlockEntity::serialize() const
{
    TagCompound compound;

    auto encodeStack = [](const ItemStack& stack) -> Tag
    {
        TagCompound slot;
        slot["item_id"]  = Tag(static_cast<uint32_t>(stack.itemID));
        slot["quantity"] = Tag(stack.quantity);
        if (stack.instanceID.has_value())
        {
            slot["instance_id"] = Tag(stack.instanceID->toString());
        }
        return Tag(slot);
    };

    compound["fuel"]   = encodeStack(fuel);
    compound["input"]  = encodeStack(input);
    compound["output"] = encodeStack(output);

    compound["burn_time_remaining"] = Tag(burn_time_remaining);
    compound["burn_time_total"]     = Tag(burn_time_total);
    compound["cook_time_remaining"] = Tag(cook_time_remaining);
    compound["cook_time_total"]     = Tag(cook_time_total);
    compound["lit"]                 = Tag(lit);

    return Tag(compound);
}

void FurnaceBlockEntity::deserialize(const Tag& tag)
{
    const auto& compound = tag.get<TagCompound>();

    auto decodeStack = [](const TagCompound& compound, const std::string& key) -> ItemStack
    {
        auto it = compound.find(key);
        if (it == compound.end()) return {ItemID::None, 0};
        const auto& slot = it->second.get<TagCompound>();

        ItemStack stack{ItemID::None, 0};
        auto id_it  = slot.find("item_id");
        auto qty_it = slot.find("quantity");
        if (id_it == slot.end() || qty_it == slot.end()) return stack;

        stack.itemID   = static_cast<ItemID>(id_it->second.get<uint32_t>());
        stack.quantity = qty_it->second.get<uint32_t>();

        if (auto iid = slot.find("instance_id"); iid != slot.end())
        {
            stack.instanceID = uuidFromString(iid->second.get<std::string>());
        }
        return stack;
    };

    fuel   = decodeStack(compound, "fuel");
    input  = decodeStack(compound, "input");
    output = decodeStack(compound, "output");

    auto getFloat = [&](const std::string& key, float fallback) -> float
    {
        auto it = compound.find(key);
        return it == compound.end() ? fallback : it->second.get<float>();
    };

    burn_time_remaining = getFloat("burn_time_remaining", 0.0f);
    burn_time_total     = getFloat("burn_time_total",     0.0f);
    cook_time_remaining = getFloat("cook_time_remaining", 0.0f);
    cook_time_total     = getFloat("cook_time_total",     0.0f);

    auto lit_it = compound.find("lit");
    lit = (lit_it != compound.end()) ? lit_it->second.get<bool>() : false;
}

std::unique_ptr<BlockEntity> FurnaceBlockEntity::clone() const
{
    auto out = std::make_unique<FurnaceBlockEntity>(position);
    out->fuel   = fuel;
    out->input  = input;
    out->output = output;
    out->burn_time_remaining = burn_time_remaining;
    out->burn_time_total     = burn_time_total;
    out->cook_time_remaining = cook_time_remaining;
    out->cook_time_total     = cook_time_total;
    out->lit                 = lit;
    out->viewers             = viewers;
    return out;
}
