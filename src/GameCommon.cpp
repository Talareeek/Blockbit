#include "../include/GameCommon.hpp"

#include "../include/World.hpp"
#include "../include/Entity.hpp"
#include "../include/TransformComponent.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

Entity& entityWithID(UUID id, World& world)
{
    if(world.getEntities().contains(id)) return world.getEntities().at(id);;

    throw std::runtime_error("No entity with UUID: " + id.toString() + " found");
}

bool doesEntityExist(UUID id, World& world)
{
    return world.getEntities().contains(id);
}

int positionToChunk(sf::Vector2<double> position)
{
    return static_cast<int>(std::floor(position.x) / 16);
}

bool isInRange(TransformComponent& player, TransformComponent& target, float range)
{
    sf::Vector2f player_closest = sf::Vector2f(player.position) + sf::Vector2f((target.position.x > player.position.x) ? static_cast<float>(player.size.x) : 0.0f, (target.position.y > player.position.y) ? static_cast<float>(player.size.y) : 0.0f);
    sf::Vector2f target_closest = sf::Vector2f(target.position) + sf::Vector2f((player.position.x > target.position.x) ? static_cast<float>(target.size.x) : 0.0f, (player.position.y > target.position.y) ? static_cast<float>(target.size.y) : 0.0f);

    float distance = std::sqrt(std::pow(player_closest.x - target_closest.x, 2) + std::pow(player_closest.y - target_closest.y, 2));

    return distance <= range;
}

bool isBlockInRange(TransformComponent& player, sf::Vector2i& block, float range)
{
    sf::Vector2f player_closest = sf::Vector2f(player.position) + sf::Vector2f((block.x > player.position.x) ? static_cast<float>(player.size.x) : 0.0f, (block.y > player.position.y) ? static_cast<float>(player.size.y) : 0.0f);
    sf::Vector2f block_closest = sf::Vector2f(block) + sf::Vector2f((player.position.x > static_cast<float>(block.x)) ? 1.0f : 0.0f, (player.position.y > static_cast<float>(block.y)) ? 1.0f : 0.0f);
    float distance = std::sqrt(std::pow(player_closest.x - block_closest.x, 2) + std::pow(player_closest.y - block_closest.y, 2));

    return distance <= range;
}
