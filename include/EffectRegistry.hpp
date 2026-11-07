#ifndef EFFECT_REGISTRY_HPP
#define EFFECT_REGISTRY_HPP

#include "EffectsComponent.hpp"

#include <functional>
#include <unordered_map>

class World;
class Entity;

struct EffectDefinition
{
    // Fired once per `instance.interval` seconds by EffectsSystem. The registry
    // is responsible for doing whatever the effect does (damaging, healing,
    // spawning particles, etc.) and flipping component `dirty` flags on
    // whatever it touched.
    std::function<void(World& world, Entity& entity, EffectInstance& instance, float dt)> tick;
};

class EffectRegistry
{
private:

    static std::unordered_map<EffectID, EffectDefinition> definitions;

public:

    static void init();

    static bool contains(EffectID id);
    static const EffectDefinition& get(EffectID id);

    static void tick(EffectID id, World& world, Entity& entity, EffectInstance& instance, float dt);
};

#endif // EFFECT_REGISTRY_HPP
