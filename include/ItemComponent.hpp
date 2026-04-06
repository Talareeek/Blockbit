#ifndef ITEM_COMPONENT_HPP
#define ITEM_COMPONENT_HPP

#include "Item.hpp"

struct ItemComponent
{
    ItemStack item;

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
};

#endif // ITEM_COMPONENT_HPP