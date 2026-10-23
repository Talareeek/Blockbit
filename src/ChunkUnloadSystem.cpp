#include "../include/ChunkUnloadSystem.hpp"
#include "../include/PlayerControlledComponent.hpp"

#include <vector>

void ChunkUnloadSystem(ServerWorld& world)
{
    std::vector<int> to_evict;
    to_evict.reserve(world.getChunks().size());

    for (const auto& [chunk_position, chunk] : world.getChunks())
    {
        if (world.chunkState(chunk_position) == ChunkState::Unloaded)
        {
            to_evict.push_back(chunk_position);
        }
    }

    for (int chunk_position : to_evict)
    {
        world.saveChunk(chunk_position);

        // Collect entities to remove from ECS before erasing the chunk — they'll
        // come back next time the chunk is loaded (loadChunkEntities re-adds).
        // Players are never evicted this way: their presence IS what keeps a
        // chunk loaded, so a player's chunk cannot be in Unloaded state.
        std::vector<UUID> entity_ids(
            world.getChunks().at(chunk_position).entity_ids.begin(),
            world.getChunks().at(chunk_position).entity_ids.end());

        for (UUID id : entity_ids)
        {
            if (!world.doesEntityExist(id)) continue;
            if (world.getEntity(id).hasComponent<PlayerControlledComponent>()) continue;
            world.removeEntity(id);
        }

        world.getChunks().erase(chunk_position);
        world.getBlockUpdatePipeline().dropChunk(chunk_position);
    }
}
