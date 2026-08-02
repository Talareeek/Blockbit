#include "../include/InventorySystem.hpp"
#include "../include/Item.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/World.hpp"

#include <SFML/Graphics.hpp>

void InventorySystem(World& world)
{
    for(auto& [id, entity] : world.getEntities())
    {
        if(!entity.hasComponent<ItemComponent>() || !entity.hasComponent<TransformComponent>()) continue;

        auto& item = entity.getComponent<ItemComponent>();
        auto& itemTransform = entity.getComponent<TransformComponent>();

        for(auto& [id, other] : world.getEntities())
        {
            if(!other.hasComponent<InventoryComponent>() || !other.hasComponent<TransformComponent>()) continue;

            auto& inventory = other.getComponent<InventoryComponent>();
            auto& inventoryTransform = other.getComponent<TransformComponent>();
            
            sf::FloatRect itemRect(sf::Vector2f(itemTransform.position), sf::Vector2f(itemTransform.size));
            sf::FloatRect inventoryRect(sf::Vector2f(inventoryTransform.position), sf::Vector2f(inventoryTransform.size));

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

    std::vector<UUID> to_erase;

    for(auto& [id, entity] : world.getEntities())
    {
        if(!entity.hasComponent<ItemComponent>()) continue;

        auto& item = entity.getComponent<ItemComponent>();

        if(item.item.empty()) to_erase.push_back(id);
    }

    for(auto& id : to_erase)
    {
        world.getEntities().erase(id);
    }
}