#include "../include/Chunk.hpp"
#include "../include/World.hpp"
#include "../include/MainGameState.hpp"

#include <iostream>

void Chunk::addEntity(Entity& entity)
{
    entity.setIndexInChunk(entities.size());
    entity.setChunk(chunk_position);
    entities.push_back(entity.getID());

    entity.setAssignedChunk(true);
}

void Chunk::removeEntity(Entity& entity, World& world)
{
    if(entities.empty() || entity.getIndexInChunk() >= entities.size())
    {
        std::cerr << "Error: Entity not found in chunk." << std::endl;
        return;
    }

    size_t index = entity.getIndexInChunk();
    uint32_t last_id = entities.back();

    entities[index] = last_id;
    entities.pop_back();
    
    if(last_id != entity.getID())
    {
        try
        {
            Entity& last_entity = entityWithID(last_id, world);
            last_entity.setIndexInChunk(index);
        }
        catch (const std::runtime_error& e)
        {
            std::cerr << "Error: Entity with ID " << last_id << " not found in world." << std::endl;
        }
        
    }

    entity.setAssignedChunk(false);
}