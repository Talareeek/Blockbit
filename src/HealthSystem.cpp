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
    std::vector<uint32_t> toDelete;


    for(auto entity : world.getEntities())
    {
        if(!entity.hasComponent<HealthComponent>()) continue;

        auto& health = entity.getComponent<HealthComponent>();

        if(health.health <= 0 && health.killOnZero)
        {
            toDelete.push_back(entity.getID());            
        }

        if(health.health <= 0)
        {
            if(!entity.hasComponent<InventoryComponent>()) continue;

            auto& inventory = entity.getComponent<InventoryComponent>();

            auto& transform = entity.getComponent<TransformComponent>();

            for(auto& item : inventory.inventory.slots)
            {
                Entity drop(world.getPossibleID());

                drop.addComponent(TransformComponent(transform.center(), {0.5, 0.5}));
                drop.addComponent(PhysicsComponent{.velocity = {randomDouble(-2.0, 2.0), randomDouble(0.0, 2.0)}});
                drop.addComponent(RenderComponent{.textureID = static_cast<uint16_t>(itemDatabase[item.itemID].texture), .size = {0.5f, 0.5f}});
                drop.addComponent(ItemComponent{.item = item});

                world.getEntities().push_back(std::move(drop));
            }
        }
    }

    world.getEntities().erase(std::remove_if(world.getEntities().begin(), world.getEntities().end(),
    [toDelete](Entity& entity)
    {
        for(auto entity_id : toDelete)
        {
            if(entity_id = entity.getID()) return true;
        }

        return false;

    }), world.getEntities().end());
}