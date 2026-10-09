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

    for(auto& [id, entity] : entities)
    {
        if(!entity.hasComponent<ExplosiveComponent>() || !entity.hasComponent<TransformComponent>()) continue;

        auto& explosive = entity.getComponent<ExplosiveComponent>();
        explosive.timer += dt;

        if(explosive.timer < explosive.fuseTime) continue;

        sf::Vector2<double> center = entity.getComponent<TransformComponent>().center();

        for(auto& [id, other] : entities)
        {
            sf::Vector2<double> diff = sf::Vector2<double>(other.getComponent<TransformComponent>().center()) - center;

            double dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

            if(dist > static_cast<double>(explosive.force) || dist == 0.0) continue;

            sf::Vector2<double> dir = diff / dist;

            double factor = 1.f - dist / explosive.force;
            factor = std::clamp(factor, 0.0, 1.0);

            float damage = factor * factor;
            if(other.hasComponent<HealthComponent>())
            {
                other.getComponent<HealthComponent>().health -= static_cast<uint8_t>(damage);
            }

            float impulse = std::sqrt(factor) * (2.0f + explosive.force * 0.5f);
            other.getComponent<PhysicsComponent>().velocity += sf::Vector2f(dir.x, dir.y - 0.6f) * impulse;
        }

        int minX = static_cast<int>(center.x - explosive.force);
        int maxX = static_cast<int>(center.x + explosive.force);
        int minY = static_cast<int>(center.y - explosive.force);
        int maxY = static_cast<int>(center.y + explosive.force);

        for(int x = minX; x <= maxX; x++)
        {
            for(int y = minY; y <= maxY; y++)
            {
                double dx = x + 0.5 - center.x;
                double dy = y + 0.5 - center.y;

                double dist = std::sqrt(dx*dx + dy*dy);

                if(dist <= explosive.force && blockDatabase[world.getBlock(x, y).id].breakable != false)
                {
                    world.setBlock(x, y, {BlockID::Air});
                }
            }
        }
    }

    std::vector<UUID> to_erase;

    for(auto& [id, entity] : world.getEntities())
    {
        if(entity.hasComponent<ExplosiveComponent>() && entity.getComponent<ExplosiveComponent>().timer >= entity.getComponent<ExplosiveComponent>().fuseTime) to_erase.push_back(id);
    }

    for(auto& id : to_erase)
    {
        world.removeEntity(id);
    }
}