#include "../include/TransformSystem.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/GameCommon.hpp"
#include "../include/Entity.hpp"
#include "../include/World.hpp"

void TransformSystem(World& world)
{
    for (uint32_t playerId : world.getPlayerEntityIDs())
    {
        auto player_position = entityWithID(playerId, world).getComponent<TransformComponent>().position;

        int player_chunk_position = player_position.x / CHUNK_WIDTH;

        int chunk_lowest = player_chunk_position - World::SIMULATION_DISTANCE / 2;
        int chunk_highest = player_chunk_position + World::SIMULATION_DISTANCE / 2;

        for (int i = chunk_lowest; i <= chunk_highest; i++)
        {
            if(world.getChunks().contains(i)) {
                if(world.getChunks()[i].generated) continue;
            }

            if(world.hasChunkFile(i)) world.readChunk(i);
            else world.generateChunk(i);
        }
    }
}
