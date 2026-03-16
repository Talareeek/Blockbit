#include "../include/HealthSystem.hpp"
#include "../include/HealthComponent.hpp"

#include <algorithm>

void healthSystem(std::vector<Entity>& entities)
{
    for (auto& entity : entities)
    {
        if (!entity.hasComponent<HealthComponent>()) continue;

        auto& health = entity.getComponent<HealthComponent>();

        entities.erase(std::remove_if(entities.begin(), entities.end(), [](Entity& e)
        {
            if (!e.hasComponent<HealthComponent>()) return false;

            const auto& health = e.getComponent<HealthComponent>();
            return health.killOnZero && health.health == 0;
        }), entities.end());
    }
}