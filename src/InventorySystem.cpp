#include "../include/InventorySystem.hpp"
#include "../include/Item.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/InventoryComponent.hpp"

#include <SFML/Graphics.hpp>

void InventorySystem(std::vector<Entity>& entities)
{
    for(auto& entity : entities)
    {
        if(!entity.hasComponent<ItemComponent>() || !entity.hasComponent<TransformComponent>()) continue;

        auto& item = entity.getComponent<ItemComponent>();
        auto& itemTransform = entity.getComponent<TransformComponent>();

        for(auto& other : entities)
        {
            if(!other.hasComponent<InventoryComponent>() || !other.hasComponent<TransformComponent>()) continue;

            auto& inventory = other.getComponent<InventoryComponent>();
            auto& inventoryTransform = other.getComponent<TransformComponent>();
            
            sf::FloatRect itemRect(itemTransform.position, itemTransform.size);
            sf::FloatRect inventoryRect(inventoryTransform.position, inventoryTransform.size);

            if(itemRect.findIntersection(inventoryRect))
            {
                item.item = inventory.inventory.addItemWithLeftover(item.item.itemID, item.item.quantity);
            }
        }

        if(item.item.empty())
        {
            entity.getComponent<ItemComponent>() = ItemComponent{{ItemID::None, 0}};
        }
    }

    entities.erase(std::remove_if(entities.begin(), entities.end(),
    [](Entity& entity)
    {
        if(!entity.hasComponent<ItemComponent>()) return false;

        auto& item = entity.getComponent<ItemComponent>();

        return item.item.empty();
    }), entities.end());
}