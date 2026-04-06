#include "../include/ExplosiveSystem.hpp"
#include "../include/World.hpp"
#include <vector>
#include "../include/ExplosiveComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/HealthComponent.hpp"

void ExplosiveSystem(World& world, float dt)
{
    auto& entities = world.getEntities();

    for(auto& entity : entities)
    {
        if(!entity.hasComponent<ExplosiveComponent>() || !entity.hasComponent<TransformComponent>()) continue;

        auto& explosive = entity.getComponent<ExplosiveComponent>();
        explosive.timer += dt;

        if(explosive.timer < explosive.fuseTime) continue;

        sf::Vector2f center = entity.getComponent<TransformComponent>().position;

        for(auto& other : entities)
        {
            sf::Vector2f diff = other.getComponent<TransformComponent>().position - center;

            float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

            if(dist > explosive.force || dist == 0) continue;

            sf::Vector2f dir = diff / dist;

            float factor = 1.f - dist / explosive.force;
            factor = std::clamp(factor, 0.f, 1.f);

            float damage = factor * factor;
            if(other.hasComponent<HealthComponent>())
            {
                other.getComponent<HealthComponent>().health -= damage;
            }

            other.getComponent<PhysicsComponent>().velocity += sf::Vector2f(dir.x, dir.y - 0.4f) * (factor);
        }

        // DESTROY BLOCKS
        int minX = static_cast<int>(center.x - explosive.force);
        int maxX = static_cast<int>(center.x + explosive.force);
        int minY = static_cast<int>(center.y - explosive.force);
        int maxY = static_cast<int>(center.y + explosive.force);

        for(int x = minX; x <= maxX; x++)
        {
            for(int y = minY; y <= maxY; y++)
            {
                float dx = x + 0.5f - center.x;
                float dy = y + 0.5f - center.y;

                float dist = std::sqrt(dx*dx + dy*dy);

                if(dist <= explosive.force)
                {
                    world.setBlock(x, y, {BlockID::Air});
                }
            }
        }
    }

    entities.erase(std::remove_if(entities.begin(), entities.end(), [](Entity& entity)
    {
        return entity.hasComponent<ExplosiveComponent>() && entity.getComponent<ExplosiveComponent>().timer >= entity.getComponent<ExplosiveComponent>().fuseTime;
    }), entities.end());
}