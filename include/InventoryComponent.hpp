#ifndef INVENTORY_COMPONENT_HPP
#define INVENTORY_COMPONENT_HPP

#include "Item.hpp"

struct InventoryComponent
{
    Inventory inventory;

    InventoryComponent(size_t size) : inventory(size) {}
};

#endif // INVENTORY_COMPONENT_HPP