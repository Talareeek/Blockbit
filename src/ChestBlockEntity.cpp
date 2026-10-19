#include "../include/ChestBlockEntity.hpp"

ChestBlockEntity::ChestBlockEntity(sf::Vector2i position) : BlockEntity(position)
{
    for (auto& stack : stacks) stack = {ItemID::None, 0};
}

void ChestBlockEntity::tick(World&, float)
{
}

Tag ChestBlockEntity::serialize() const
{
    TagCompound compound;

    TagList slot_list;
    slot_list.reserve(stacks.size());

    for (const auto& stack : stacks)
    {
        TagCompound slot;
        slot["item_id"] = Tag(static_cast<uint32_t>(stack.itemID));
        slot["quantity"] = Tag(stack.quantity);
        if (stack.instanceID.has_value())
        {
            slot["instance_id"] = Tag(stack.instanceID->toString());
        }
        slot_list.push_back(Tag(slot));
    }

    compound["stacks"] = Tag(slot_list);

    return Tag(compound);
}

void ChestBlockEntity::deserialize(const Tag& tag)
{
    for (auto& stack : stacks) stack = {ItemID::None, 0};

    const auto& compound = tag.get<TagCompound>();
    auto it = compound.find("stacks");
    if (it == compound.end()) return;

    const auto& slot_list = it->second.get<TagList>();

    const size_t count = std::min(slot_list.size(), stacks.size());

    for (size_t i = 0; i < count; ++i)
    {
        const auto& slot = slot_list[i].get<TagCompound>();

        auto id_it = slot.find("item_id");
        auto qty_it = slot.find("quantity");
        if (id_it == slot.end() || qty_it == slot.end()) continue;

        stacks[i].itemID = static_cast<ItemID>(id_it->second.get<uint32_t>());
        stacks[i].quantity = qty_it->second.get<uint32_t>();

        stacks[i].instanceID.reset();
        if (auto iid = slot.find("instance_id"); iid != slot.end())
        {
            stacks[i].instanceID = uuidFromString(iid->second.get<std::string>());
        }
    }
}

std::unique_ptr<BlockEntity> ChestBlockEntity::clone() const
{
    auto out = std::make_unique<ChestBlockEntity>(position);
    out->stacks = stacks;
    out->viewers = viewers;
    return out;
}
