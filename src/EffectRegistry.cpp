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
        .display_name = "Regeneration",
        .liquid_color = sf::Color(255, 120, 200),
        .tick = [](World&, Entity& entity, EffectInstance& instance, float)
        {
            healEntity(entity, instance.magnitude);
        }
    };

    definitions[EffectID::Poison] = EffectDefinition
    {
        .display_name = "Poison",
        .liquid_color = sf::Color(110, 220, 70),
        .tick = [](World&, Entity& entity, EffectInstance& instance, float)
        {
            damageEntity(entity, instance.magnitude);
        }
    };

    definitions[EffectID::Burning] = EffectDefinition
    {
        .display_name = "Burning",
        .liquid_color = sf::Color(255, 120, 40),
        .tick = [](World&, Entity& entity, EffectInstance& instance, float)
        {
            damageEntity(entity, instance.magnitude);
        }
    };

    definitions[EffectID::Starving] = EffectDefinition
    {
        .display_name = "Starving",
        .liquid_color = sf::Color(170, 120, 60),
        .tick = [](World&, Entity& entity, EffectInstance& instance, float)
        {
            damageEntity(entity, instance.magnitude);
        }
    };

    definitions[EffectID::BoundaryDistortion] = EffectDefinition
    {
        .display_name = "Boundary Distortion",
        .liquid_color = sf::Color(200, 80, 220),
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

    // Speed and JumpBoost don't tick — their effect is applied as a
    // per-frame multiplier in Input.cpp by looking up the EffectsComponent.
    // A definition is still registered so EffectRegistry::contains() returns
    // true and the duration-sweep logic in EffectsSystem runs.
    definitions[EffectID::Speed] = EffectDefinition
    {
        .display_name = "Speed",
        .liquid_color = sf::Color(80, 220, 255),
        .tick = [](World&, Entity&, EffectInstance&, float){}
    };
    definitions[EffectID::JumpBoost] = EffectDefinition
    {
        .display_name = "Jump Boost",
        .liquid_color = sf::Color(255, 180, 80),
        .tick = [](World&, Entity&, EffectInstance&, float){}
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
