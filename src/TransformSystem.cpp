#include "../include/TransformSystem.hpp"
#include "../include/TransformComponent.hpp"

void TransformSystem(World& world)
{
    for(auto& entity : world.getEntities())
    {
        if(!entity.hasComponent<TransformComponent>()) continue;

        auto& transform = entity.getComponent<TransformComponent>();
        
        int position_chunk = static_cast<int>(std::floor(transform.position.x / CHUNK_WIDTH));

        if(position_chunk != entity.getChunk())
        {
            if(entity.getAssignedChunk())
            {
                world.getChunk(entity.getChunk()).removeEntity(entity, world);
            }
            world.getChunk(position_chunk).addEntity(entity);
        }

        if(transform.position.y < -1000.0f)
        {
            world.getChunk(entity.getChunk()).removeEntity(entity, world);
        }
    }

    world.getEntities().erase(std::remove_if(world.getEntities().begin(), world.getEntities().end(), [](Entity& e) { if(!e.hasComponent<TransformComponent>()) return false; return !e.getAssignedChunk(); }), world.getEntities().end());
}