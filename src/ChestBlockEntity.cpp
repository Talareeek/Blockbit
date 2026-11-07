#include "../include/ChestBlockEntity.hpp"
#include "../include/ItemUtils.hpp"

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
        slot_list.push_back(stackToTag(stack));
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
        stacks[i] = stackFromTag(slot_list[i], /*write_instance_data=*/true);
    }
}

std::unique_ptr<BlockEntity> ChestBlockEntity::clone() const
{
    auto out = std::make_unique<ChestBlockEntity>(position);
    out->stacks = stacks;
    out->viewers = viewers;
    return out;
}
