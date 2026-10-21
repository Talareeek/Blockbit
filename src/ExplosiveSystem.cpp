#include "../include/ExplosiveSystem.hpp"
#include "../include/World.hpp"
#include "../include/ExplosiveComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/Prefab.hpp"

#include <vector>
#include <random>
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float BLOCK_DROP_CHANCE = 0.35f;
    constexpr float RADIUS_JITTER = 0.6f;
}

void ExplosiveSystem(World& world, float dt)
{
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> jitter_dist(-RADIUS_JITTER, RADIUS_JITTER);
    std::uniform_real_distribution<float> unit_dist(0.0f, 1.0f);

    auto& entities = world.getEntities();

    struct PendingBlockBreak
    {
        sf::Vector2i position;
        BlockID id;
        bool drop_loot;
    };
    struct PendingDrop
    {
        sf::Vector2<double> position;
        ItemStack stack;
    };

    std::vector<PendingBlockBreak> block_breaks;
    std::vector<PendingDrop> drops;
    std::vector<UUID> to_erase;

    for(auto& [id, entity] : entities)
    {
        if(!entity.hasComponent<ExplosiveComponent>() || !entity.hasComponent<TransformComponent>()) continue;

        auto& explosive = entity.getComponent<ExplosiveComponent>();
        explosive.timer += dt;
        explosive.dirty = true;

        if(explosive.timer < explosive.fuseTime) continue;

        to_erase.push_back(id);

        const sf::Vector2<double> center = entity.getComponent<TransformComponent>().center();
        const float force = explosive.force;

        if(explosive.damageEntities)
        {
            for(auto& [other_id, other] : entities)
            {
                if(other_id == id) continue;
                if(!other.hasComponent<TransformComponent>()) continue;

                sf::Vector2<double> diff = other.getComponent<TransformComponent>().center() - center;
                double dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

                if(dist > static_cast<double>(force) || dist == 0.0) continue;

                sf::Vector2<double> dir = diff / dist;

                double factor = std::clamp(1.0 - dist / force, 0.0, 1.0);

                if(other.hasComponent<HealthComponent>())
                {
                    auto& other_health = other.getComponent<HealthComponent>();
                    float damage = static_cast<float>(factor) * (20.0f + force * 10.0f);
                    damage *= 0.75f + unit_dist(rng) * 0.5f;
                    uint8_t dmg = static_cast<uint8_t>(std::min<float>(damage, 255.0f));
                    other_health.health = (other_health.health > dmg) ? static_cast<uint8_t>(other_health.health - dmg) : 0;
                    other_health.since_last_damaged = 0.0f;
                    other_health.dirty = true;
                }

                if(other.hasComponent<PhysicsComponent>())
                {
                    float impulse = std::sqrt(static_cast<float>(factor)) * (2.0f + force * 0.5f);
                    impulse *= 0.85f + unit_dist(rng) * 0.3f;
                    auto& other_physics = other.getComponent<PhysicsComponent>();
                    other_physics.velocity += sf::Vector2f(dir.x, dir.y - 0.6f) * impulse;
                    other_physics.dirty = true;
                }
            }
        }

        if(!explosive.destroyBlocks) continue;

        const int margin = static_cast<int>(std::ceil(force + RADIUS_JITTER)) + 1;
        const int minX = static_cast<int>(std::floor(center.x)) - margin;
        const int maxX = static_cast<int>(std::floor(center.x)) + margin;
        const int minY = static_cast<int>(std::floor(center.y)) - margin;
        const int maxY = static_cast<int>(std::floor(center.y)) + margin;

        for(int x = minX; x <= maxX; x++)
        {
            for(int y = minY; y <= maxY; y++)
            {
                double dx = x + 0.5 - center.x;
                double dy = y + 0.5 - center.y;
                double dist = std::sqrt(dx * dx + dy * dy);

                float effective_radius = force + jitter_dist(rng);
                if(dist > effective_radius) continue;

                Block block = world.getBlock(x, y);
                if(block.id == BlockID::Air) continue;

                auto& block_data = blockDatabase[block.id];
                if(!block_data.breakable) continue;

                bool should_drop = block_data.drop.has_value() && unit_dist(rng) < BLOCK_DROP_CHANCE;
                block_breaks.push_back({{x, y}, block.id, should_drop});

                if(should_drop)
                {
                    const auto& drop_props = block_data.drop.value();
                    std::uniform_int_distribution<int> amount_range(drop_props.min_amount, drop_props.max_amount);
                    ItemStack stack = {drop_props.drop, static_cast<uint32_t>(amount_range(rng))};
                    drops.push_back({{x + 0.5, y + 0.5}, stack});
                }
            }
        }
    }

    for(const auto& brk : block_breaks)
    {
        if(world.getBlock(brk.position.x, brk.position.y).id == brk.id)
        {
            world.setBlock(brk.position.x, brk.position.y, {BlockID::Air, 0});
        }
    }

    for(const auto& drop : drops)
    {
        spawnDroppedItem(world, drop.position, drop.stack, randomDropSpread(90.0f));
    }

    for(auto& id : to_erase)
    {
        world.removeEntity(id);
    }
}
