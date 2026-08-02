#include "../include/ChunkUnloadSystem.hpp"
#include "../include/GameCommon.hpp"
#include "../include/Entity.hpp"
#include "../include/TransformComponent.hpp"

static bool isChunkSafe(int chunkPos, const std::vector<UUID>& playerIds, World& world)
{
    for (UUID playerId : playerIds)
    {
        int player_chunk = world.getEntity(playerId).getComponent<TransformComponent>().position.x / CHUNK_WIDTH;
        int min_safe = player_chunk - World::SIMULATION_DISTANCE / 2;
        int max_safe = player_chunk + World::SIMULATION_DISTANCE / 2;
        if (chunkPos >= min_safe && chunkPos <= max_safe) return true;
    }
    return false;
}

void ChunkUnloadSystem(World& world)
{
    if(world.getChunks().size() <= World::MAX_CHUNKS_LOADED) return;

    auto playerIds = world.getPlayerEntityIDs();

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

        bool erased = false;

        if(!isChunkSafe(min, playerIds, world))
        {
            world.saveChunk(min);
            world.getChunks().erase(min);
            erased = true;
        }
        if(world.getChunks().size() > World::PREFFERED_CHUNKS_LOADED && max != min && !isChunkSafe(max, playerIds, world))
        {
            world.saveChunk(max);
            world.getChunks().erase(max);
            erased = true;
        }

        if(!erased) break;
    }
}
