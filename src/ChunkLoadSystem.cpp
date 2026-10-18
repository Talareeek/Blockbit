#include "../include/ChunkLoadSystem.hpp"
#include "../include/TransformComponent.hpp"

#include <cmath>

void ChunkLoadSystem(ServerWorld& world, float dt)
{
    auto position_to_chunk = [](sf::Vector2<double> position) -> int
    {
        return static_cast<int>(std::floor(position.x / 16.0));
    };

    for(auto& [id, entity] : world.getEntities())
    {
        if(!entity.hasComponent<TransformComponent>()) continue;

        const auto& t = entity.getComponent<TransformComponent>();
        const int left_chunk  = position_to_chunk(t.position);
        const int right_chunk = position_to_chunk({t.position.x + t.size.x, t.position.y});

        world.loadOrCreateChunk(left_chunk);
        if(right_chunk != left_chunk) world.loadOrCreateChunk(right_chunk);
    }
}