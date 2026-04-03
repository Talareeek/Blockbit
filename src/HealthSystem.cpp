#include "../include/HealthSystem.hpp"
#include "../include/HealthComponent.hpp"

#include <algorithm>

void HealthSystem(World& world)
{
    std::vector<uint32_t> entitiesToDelete;

    for (auto& entity : world.getEntities())
    {
        if (!entity.hasComponent<HealthComponent>()) continue;

        auto& health = entity.getComponent<HealthComponent>();

        if(health.killOnZero && health.health == 0)
        {
            entitiesToDelete.push_back(entity.getID());
        }
    }

    for (uint32_t id : entitiesToDelete)
    {
        world.deleteEntity(id);
    }
}