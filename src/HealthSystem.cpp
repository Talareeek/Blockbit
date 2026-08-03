#include "../include/HealthSystem.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"

#include <algorithm>

std::random_device rd;

float randomDouble(double min, double max)
{
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dist(min, max);

    return dist(gen);
}

void HealthSystem(World& world)
{
    std::vector<UUID> toDelete;


    for(auto& [id, entity] : world.getEntities())
    {
        if(!entity.hasComponent<HealthComponent>()) continue;

        auto& health = entity.getComponent<HealthComponent>();

        if(health.health <= 0 && health.killOnZero)
        {
            toDelete.push_back(entity.getID());

            if(!entity.hasComponent<InventoryComponent>()) continue;

            auto& inventory = entity.getComponent<InventoryComponent>();

            auto& transform = entity.getComponent<TransformComponent>();

            for(auto& item : inventory.inventory.slots)
            {
                Entity drop(generateUUID());

                drop.addComponent(TransformComponent(transform.center(), {0.5, 0.5}));
                PhysicsComponent{{randomDouble(-2.0, 2.0), randomDouble(0.0, 2.0)}};
                RenderComponent{static_cast<uint16_t>(itemDatabase[item.itemID].texture), {}, {0.5f,0.5f}};
                drop.addComponent(ItemComponent{item});

                world.addEntity(std::move(drop));
            }
        }
    }

    for(auto& id : toDelete)
    {
        world.removeEntity(id);
    }
}