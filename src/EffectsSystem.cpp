#include "../include/EffectsSystem.hpp"
#include "../include/EffectsComponent.hpp"
#include "../include/EffectRegistry.hpp"
#include "../include/TransformComponent.hpp"

#include <algorithm>
#include <vector>

void EffectsSystem(ServerWorld& world, float dt)
{
    for (auto& [id, entity] : world.getEntities())
    {
        if (!entity.hasComponent<EffectsComponent>()) continue;

        if (entity.hasComponent<TransformComponent>()
            && !world.isChunkLoaded(entity.getComponent<TransformComponent>().chunkPosition())) continue;

        auto& effects = entity.getComponent<EffectsComponent>();
        if (effects.effects.empty()) continue;

        bool mutated = false;

        for (auto& instance : effects.effects)
        {
            instance.since_last_tick += dt;

            if (instance.since_last_tick >= instance.interval)
            {
                EffectRegistry::tick(instance.id, world, entity, instance, dt);
                instance.since_last_tick = 0.0f;
                mutated = true;
            }

            // remaining_duration < 0 is the "infinite" sentinel and is never
            // decremented, so finite effects clamp to 0 and get swept below.
            if (instance.remaining_duration > 0.0f)
            {
                instance.remaining_duration = std::max(0.0f, instance.remaining_duration - dt);
                mutated = true;
            }
        }

        const size_t before = effects.effects.size();
        effects.effects.erase(
            std::remove_if(effects.effects.begin(), effects.effects.end(),
                [](const EffectInstance& e) { return e.remaining_duration == 0.0f; }),
            effects.effects.end());

        if (effects.effects.size() != before) mutated = true;

        if (mutated) effects.dirty = true;
    }
}
