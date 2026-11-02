#include "../include/ChunkLoadSystem.hpp"
#include "../include/PlayerControlledComponent.hpp"
#include "../include/TransformComponent.hpp"

void ChunkLoadSystem(ServerWorld& world, float /*dt*/)
{
    for (auto& [id, entity] : world.getEntities())
    {
        if (!entity.hasComponent<PlayerControlledComponent>()) continue;
        if (!entity.hasComponent<TransformComponent>()) continue;

        int player_chunk = entity.getComponent<TransformComponent>().chunkPosition();

        for (int offset = -ServerWorld::LAZY_DISTANCE; offset <= ServerWorld::LAZY_DISTANCE; ++offset)
        {
            world.loadOrCreateChunk(player_chunk + offset);
        }
    }

    world.refreshChunkStates();
}
