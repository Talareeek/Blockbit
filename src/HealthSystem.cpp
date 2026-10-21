#include "../include/HealthSystem.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/Prefab.hpp"

#include <algorithm>

void HealthSystem(World& world, float dt)
{
    std::vector<UUID> toDelete;

    struct PendingDrop
    {
        sf::Vector2<double> position;
        ItemStack stack;
    };
    std::vector<PendingDrop> pending_drops;

    for(auto& [id, entity] : world.getEntities())
    {
        if(!entity.hasComponent<HealthComponent>()) continue;

        auto& health = entity.getComponent<HealthComponent>();

        if(health.since_last_damaged < HealthComponent::DAMAGE_FLASH_DURATION)
        {
            health.since_last_damaged += dt;
            if(health.since_last_damaged > HealthComponent::DAMAGE_FLASH_DURATION)
            {
                health.since_last_damaged = HealthComponent::DAMAGE_FLASH_DURATION;
            }
            health.dirty = true;
        }

        if(health.health <= 0)
        {
            toDelete.push_back(entity.getID());

            if(!entity.hasComponent<InventoryComponent>() || !entity.hasComponent<TransformComponent>()) continue;

            auto& inventory = entity.getComponent<InventoryComponent>();
            auto& transform = entity.getComponent<TransformComponent>();

            for(auto& item : inventory.inventory.slots)
            {
                if(item.empty()) continue;
                pending_drops.push_back({transform.center(), item});
            }
        }
    }

    for(const auto& drop : pending_drops)
    {
        spawnDroppedItem(world, drop.position, drop.stack, randomDropSpread(60.0f));
    }

    for(auto& id : toDelete)
    {
        world.removeEntity(id);
    }
}