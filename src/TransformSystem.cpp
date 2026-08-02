#include "../include/TransformSystem.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/GameCommon.hpp"
#include "../include/Entity.hpp"
#include "../include/World.hpp"
#include "../include/PlayerControlledComponent.hpp"

void TransformSystem(World& world)
{
    for(auto& [id, entity] : world.getEntities())
    {
        if(!entity.hasComponent<TransformComponent>()) continue;

        auto& transform = entity.getComponent<TransformComponent>();

        if(transform.moved())
        {
            if(transform.movedBetweenChunks())
            {
                int previous_chunk = transform.previousChunkPosition();
                int current_chunk = transform.chunkPosition();

                world.getChunk(previous_chunk).entity_ids.erase(id);
                world.getChunk(previous_chunk).dirty = true;
                world.getChunk(current_chunk).entity_ids.insert(id);
                world.getChunk(current_chunk).dirty = true;
            }

            transform.previous_position = transform.position;
        }

        if(transform.position.y < -50 && entity.hasComponent<HealthComponent>())
        {
            auto& health = entity.getComponent<HealthComponent>();

            health.health -= 1;
        }

        if(entity.hasComponent<PlayerControlledComponent>())
        {
            int chunk_lowest = transform.chunkPosition() - World::SIMULATION_DISTANCE / 2;
            int chunk_highest = transform.chunkPosition() + World::SIMULATION_DISTANCE / 2;

            for (int i = chunk_lowest; i <= chunk_highest; i++)
            {
                world.loadOrCreateChunk(i);
            }
        }
    }
}
