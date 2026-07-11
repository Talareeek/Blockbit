#include "../include/ChunkLoadSystem.hpp"
#include "../include/TransformComponent.hpp"

void ChunkLoadSystem(World& world, float dt)
{
    auto position_to_chunk = [](sf::Vector2<double> position) -> int
    {
        return static_cast<int>(position.x / 16.0);
    };

    for(auto& entity : world.getEntities())
    {
        world.loadOrCreateChunk(position_to_chunk(entity.getComponent<TransformComponent>().position));
    }
}