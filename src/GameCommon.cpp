#include "../include/GameCommon.hpp"

#include "../include/World.hpp"
#include "../include/Entity.hpp"
#include "../include/TransformComponent.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

Entity& entityWithID(uint32_t id, World& world)
{
    for(auto& entity : world.getEntities())
    {
        if(id == entity.getID()) return entity;
    }

    throw std::runtime_error("Entity with ID " + std::to_string(id) + " does not exist (entityWithID)");
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
