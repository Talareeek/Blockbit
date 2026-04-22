#include "../include/ChunkUnloadSystem.hpp"
#include "../include/MainGameState.hpp"

void ChunkUnloadSystem(World& world)
{
    if(world.getChunks().size() <= World::MAX_CHUNKS_LOADED) return;

    while(world.getChunks().size() > World::PREFFERED_CHUNKS_LOADED)
    {
        std::vector<int> keys;
        for(auto& a : world.getChunks())
        {
            keys.push_back(a.first);
        }

        if(keys.empty()) break;

        int min = keys[0];
        int max = keys[0];

        for(auto& a : keys)
        {
            if(a > max) max = a;
            if(a < min) min = a;
        }

        int player_chunk_position = entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.x / CHUNK_WIDTH;

        int min_safe = player_chunk_position - World::SIMULATION_DISTANCE / 2;
        int max_safe = player_chunk_position - World::SIMULATION_DISTANCE / 2;

        if(min < min_safe)
        {
            world.writeChunk(min);
            world.getChunks().erase(min);
        }
        if(world.getChunks().size() > World::PREFFERED_CHUNKS_LOADED && max > max_safe)
        {
            world.writeChunk(max);
            world.getChunks().erase(max);
        }
    }
}