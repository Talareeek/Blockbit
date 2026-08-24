#ifndef INVENTORY_COMPONENT_HPP
#define INVENTORY_COMPONENT_HPP

#include "Component.hpp"

#include "Item.hpp"

#include <cstdint>
#include <string>
#include <sstream>

struct InventoryComponent : public Component
{
    Inventory inventory;
    uint8_t selectedSlot = 0;

    InventoryComponent(size_t size) : inventory(size) {}

    std::string serialize()
    {
        std::string output;
        output += std::to_string(inventory.slots.size()) + ' ' + std::to_string(static_cast<uint32_t>(selectedSlot)) + '\n';

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

        if(std::getline(iss, line))
        {
            std::istringstream firstLine(line);
            size_t size = 0;
            uint32_t slot = 0;
            firstLine >> size >> slot;
            selectedSlot = static_cast<uint8_t>(slot);
            if(inventory.slots.size() != size)
            {
                inventory.slots.resize(size, {ItemID::None, 0});
            }
        }

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

    std::string name() const override
    {
        return "InventoryComponent";
    }

    Tag serialize() const override
    {
        TagCompound compound;

        compound["selected_slot"] = Tag(selectedSlot);

        compound["slots"] = TagList();


        for(const auto& slot : inventory.slots)
        {
            TagCompound slot_compound;
            slot_compound["item_id"] = Tag(static_cast<uint32_t>(slot.itemID));
            slot_compound["quantity"] = Tag(slot.quantity);

            compound["slots"].get<TagList>().push_back(Tag(slot_compound));
        }

        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        selectedSlot = tag["selected_slot"].get<uint8_t>();

        const auto& slots = tag["slots"].get<TagList>();

        if (inventory.slots.size() != slots.size())
        {
            inventory.slots.resize(slots.size(), {ItemID::None, 0});
        }

        for (size_t i = 0; i < slots.size(); ++i)
        {
            const auto& slot = slots[i].get<TagCompound>();

            inventory.slots[i].itemID = static_cast<ItemID>(slot.at("item_id").get<uint32_t>());
            inventory.slots[i].quantity = slot.at("quantity").get<uint32_t>();
        }
    }

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<InventoryComponent>(*this);
    }
};

#endif // INVENTORY_COMPONENT_HPP