#ifndef EFFECT_REGISTRY_HPP
#define EFFECT_REGISTRY_HPP

#include "EffectsComponent.hpp"

#include <SFML/Graphics/Color.hpp>

#include <functional>
#include <string>
#include <unordered_map>

class World;
class Entity;

struct EffectDefinition
{
    // Human-readable name used in UI (tooltip lines, chat). Empty means the
    // effect is anonymous and tooltips fall back to the numeric id.
    std::string display_name;

    // Colour used for the tinted-liquid pass in bottle rendering (Slot.cpp).
    // Harmless for effects that never ride on a bottle.
    sf::Color liquid_color = sf::Color::White;

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
