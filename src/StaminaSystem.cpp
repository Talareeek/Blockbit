#include "../include/StaminaSystem.hpp"
#include "../include/StaminaComponent.hpp"
#include "../include/TransformComponent.hpp"

#include <algorithm>

void StaminaSystem(ServerWorld& world, float dt)
{
    for(auto& [id, entity] : world.getEntities())
    {
        if(!entity.hasComponent<StaminaComponent>()) continue;

        if(entity.hasComponent<TransformComponent>()
           && !world.isChunkLoaded(entity.getComponent<TransformComponent>().chunkPosition())) continue;

        auto& stamina = entity.getComponent<StaminaComponent>();

        if(stamina.sprint_requested && stamina.stamina > 0.0f)
        {
            stamina.stamina = std::max(0.0f, stamina.stamina - StaminaComponent::DRAIN_RATE * dt);
            stamina.since_last_drain = 0.0f;
            stamina.dirty = true;
        }
        else
        {
            if(stamina.since_last_drain < StaminaComponent::REGEN_DELAY)
            {
                stamina.since_last_drain += dt;
            }
            else if(stamina.stamina < stamina.maxStamina)
            {
                stamina.stamina = std::min(stamina.maxStamina, stamina.stamina + StaminaComponent::REGEN_RATE * dt);
                stamina.dirty = true;
            }
        }

        stamina.sprint_requested = false;
    }
}
