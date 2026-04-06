#include "../include/HealthSystem.hpp"
#include "../include/HealthComponent.hpp"

#include <algorithm>

void HealthSystem(World& world)
{
    world.getEntities().erase(std::remove_if(world.getEntities().begin(), world.getEntities().end(),
    [](Entity& entity)
    {
        if(!entity.hasComponent<HealthComponent>()) return false;

        auto& health = entity.getComponent<HealthComponent>();

        if(health.health <= 0 && health.killOnZero)
        {
            return true;
        }
        return false;
    }), world.getEntities().end());
}