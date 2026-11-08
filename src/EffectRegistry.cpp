#include "../include/EffectRegistry.hpp"
#include "../include/Entity.hpp"
#include "../include/HealthComponent.hpp"

std::unordered_map<EffectID, EffectDefinition> EffectRegistry::definitions;

static void damageEntity(Entity& entity, uint32_t amount)
{
    if (!entity.hasComponent<HealthComponent>()) return;

    auto& health = entity.getComponent<HealthComponent>();
    if (!health.damageable) return;

    health.health = (amount >= health.health) ? 0 : health.health - amount;
    health.since_last_damaged = 0.0f;
    health.dirty = true;
}

static void healEntity(Entity& entity, uint32_t amount)
{
    if (!entity.hasComponent<HealthComponent>()) return;

    auto& health = entity.getComponent<HealthComponent>();
    health.health = std::min(health.health + amount, health.maxHealth);
    health.dirty = true;
}

void EffectRegistry::init()
{
    definitions[EffectID::Regeneration] = EffectDefinition
    {
        .tick = [](World&, Entity& entity, EffectInstance& instance, float)
        {
            healEntity(entity, instance.magnitude);
        }
    };

    definitions[EffectID::Poison] = EffectDefinition
    {
        .tick = [](World&, Entity& entity, EffectInstance& instance, float)
        {
            damageEntity(entity, instance.magnitude);
        }
    };

    definitions[EffectID::Burning] = EffectDefinition
    {
        .tick = [](World&, Entity& entity, EffectInstance& instance, float)
        {
            damageEntity(entity, instance.magnitude);
        }
    };

    definitions[EffectID::Starving] = EffectDefinition
    {
        .tick = [](World&, Entity& entity, EffectInstance& instance, float)
        {
            damageEntity(entity, instance.magnitude);
        }
    };

    definitions[EffectID::BoundaryDistortion] = EffectDefinition
    {
        .tick = [](World&, Entity& entity, EffectInstance& instance, float)
        {
            if (!entity.hasComponent<HealthComponent>()) return;
            auto& health = entity.getComponent<HealthComponent>();

            const uint32_t floor = 1;
            const uint32_t drop  = instance.magnitude;
            health.maxHealth = (drop >= health.maxHealth || health.maxHealth - drop < floor)
                ? floor
                : health.maxHealth - drop;
            if (health.health > health.maxHealth) health.health = health.maxHealth;
            health.dirty = true;
        }
    };
}

bool EffectRegistry::contains(EffectID id)
{
    return definitions.count(id) != 0;
}

const EffectDefinition& EffectRegistry::get(EffectID id)
{
    return definitions.at(id);
}

void EffectRegistry::tick(EffectID id, World& world, Entity& entity, EffectInstance& instance, float dt)
{
    auto it = definitions.find(id);
    if (it == definitions.end() || !it->second.tick) return;

    it->second.tick(world, entity, instance, dt);
}
