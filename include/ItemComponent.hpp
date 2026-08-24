#ifndef ITEM_COMPONENT_HPP
#define ITEM_COMPONENT_HPP

#include "Component.hpp"

#include "Item.hpp"

struct ItemComponent : public Component
{
    ItemStack item;

    ItemComponent(ItemStack item = {})
        : item(item)
    {
    }

    std::string serialize()
    {
        std::string output;

        output += std::to_string(static_cast<uint32_t>(item.itemID)) + ' ';
        output += std::to_string(item.quantity) + '\n';

        return output;
    }

    void deserialize(const std::string& data)
    {
        std::istringstream iss(data);
        uint32_t itemIDInt;
        uint32_t quantity;

        iss >> itemIDInt >> quantity;

        item.itemID = static_cast<ItemID>(itemIDInt);
        item.quantity = quantity;
    }

    std::string name() const override
    {
        return "ItemComponent";
    }

    Tag serialize() const override
    {
        TagCompound compound;

        compound["item_id"] = Tag(static_cast<uint32_t>(item.itemID));
        compound["quantity"] = Tag(item.quantity);

        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        item.itemID = static_cast<ItemID>(tag["item_id"].get<uint32_t>());
        item.quantity = tag["quantity"].get<uint32_t>();
    }

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<ItemComponent>(*this);
    }
};

#endif // ITEM_COMPONENT_HPP