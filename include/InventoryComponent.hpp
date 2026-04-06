#ifndef INVENTORY_COMPONENT_HPP
#define INVENTORY_COMPONENT_HPP

#include "Item.hpp"

#include <string>
#include <sstream>

struct InventoryComponent
{
    Inventory inventory;

    InventoryComponent(size_t size) : inventory(size) {}

    std::string serialize()
    {
        std::string output;

        for(const auto& slot : inventory.slots)
        {
            output += std::to_string(static_cast<uint32_t>(slot.itemID)) + ' ';
            output += std::to_string(slot.quantity) + '\n';
        }

        return output;
    }

    void deserialize(const std::string& data)
    {
        std::istringstream iss(data);
        std::string line;
        size_t index = 0;

        while(std::getline(iss, line) && index < inventory.slots.size())
        {
            std::istringstream lineStream(line);
            uint32_t itemIDInt;
            uint32_t quantity;

            lineStream >> itemIDInt >> quantity;

            inventory.slots[index].itemID = static_cast<ItemID>(itemIDInt);
            inventory.slots[index].quantity = quantity;

            index++;
        }
    }
};

#endif // INVENTORY_COMPONENT_HPP