#include "../include/TransformSystem.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/MainGameState.hpp"

void TransformSystem(World& world)
{
    auto player_position = entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position;

    // UNLOADING CHUNKS
    int player_chunk_position = player_position.x / CHUNK_WIDTH;

    int chunk_lowest = player_chunk_position - World::SIMULATION_DISTANCE / 2;
    int chunk_highest = player_chunk_position + World::SIMULATION_DISTANCE / 2;

    for (int i = chunk_lowest; i <= chunk_highest; i++)
    {
        if(world.getChunks().contains(i) && world.getChunk(i).generated) continue;

        if(world.hasChunkFile(i)) world.readChunk(i);
        else world.generateChunk(i);
    }
}