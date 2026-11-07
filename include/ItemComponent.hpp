#ifndef ITEM_COMPONENT_HPP
#define ITEM_COMPONENT_HPP

#include "Component.hpp"

#include "Item.hpp"
#include "ItemUtils.hpp"

struct ItemComponent : public Component
{
    ItemStack item;

    ItemComponent(ItemStack item = {})
        : item(item)
    {
    }

    std::string name() const override
    {
        return "ItemComponent";
    }

    Tag serialize() const override
    {
        return stackToTag(item);
    }

    void deserialize(const Tag& tag) override
    {
        item = stackFromTag(tag, /*write_instance_data=*/true);
    }

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<ItemComponent>(*this);
    }
};

#endif // ITEM_COMPONENT_HPP
