#include "../include/InventorySystem.hpp"
#include "../include/Item.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/World.hpp"

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <vector>

void InventorySystem(World& world)
{
    {
        std::vector<UUID> item_ids;
        for(auto& [id, entity] : world.getEntities())
        {
            if(!entity.hasComponent<ItemComponent>() || !entity.hasComponent<TransformComponent>()) continue;
            if(entity.getComponent<ItemComponent>().item.empty()) continue;
            item_ids.push_back(id);
        }

        constexpr double MERGE_DISTANCE_SQ = 0.5 * 0.5;

        for(size_t i = 0; i < item_ids.size(); ++i)
        {
            if(!world.doesEntityExist(item_ids[i])) continue;
            auto& a = world.getEntity(item_ids[i]);
            auto& aItem = a.getComponent<ItemComponent>();
            if(aItem.item.empty()) continue;

            uint32_t maxStack = itemDatabase[aItem.item.itemID].maxStackSize;

            for(size_t j = i + 1; j < item_ids.size(); ++j)
            {
                if(aItem.item.quantity >= maxStack) break;
                if(!world.doesEntityExist(item_ids[j])) continue;

                auto& b = world.getEntity(item_ids[j]);
                auto& bItem = b.getComponent<ItemComponent>();
                if(bItem.item.empty()) continue;
                if(!canStack(aItem.item, bItem.item)) continue;

                auto& aTransform = a.getComponent<TransformComponent>();
                auto& bTransform = b.getComponent<TransformComponent>();

                sf::Vector2<double> aCenter = aTransform.center();
                sf::Vector2<double> bCenter = bTransform.center();
                sf::Vector2<double> diff = aCenter - bCenter;
                double distSq = diff.x * diff.x + diff.y * diff.y;
                if(distSq > MERGE_DISTANCE_SQ) continue;

                uint32_t space = maxStack - aItem.item.quantity;
                uint32_t move = std::min(space, bItem.item.quantity);

                aItem.item.quantity += move;
                bItem.item.quantity -= move;

                sf::Vector2<double> midCenter = (aCenter + bCenter) / 2.0;
                aTransform.teleport(midCenter - aTransform.size / 2.0);

                aItem.dirty = true;
                bItem.dirty = true;

                if(bItem.item.quantity == 0)
                {
                    bItem.item = ItemStack{ItemID::None, 0};
                }
            }
        }
    }

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
                item.dirty = true;
                inventory.dirty = true;
            }
        }

        if(item.item.empty())
        {
            entity.getComponent<ItemComponent>() = ItemComponent{{ItemID::None, 0}};
            item.dirty = true;
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
        world.removeEntity(id);
    }
}