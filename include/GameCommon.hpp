#ifndef GAME_COMMON_HPP
#define GAME_COMMON_HPP

#include <cstdint>
#include <SFML/System.hpp>

class World;
class Entity;
struct TransformComponent;

Entity& entityWithID(uint32_t id, World& world);

bool isInRange(TransformComponent& player, TransformComponent& target, float range);
bool isBlockInRange(TransformComponent& player, sf::Vector2i& block, float range);

constexpr unsigned int WORLD_UNIT_SIZE_FACTOR = 12;
constexpr uint8_t WORLD_TICKS_PER_SECOND = 60;

#endif // GAME_COMMON_HPP
