#ifndef GAME_COMMON_HPP
#define GAME_COMMON_HPP

#include "UUID.hpp"

#include <cstdint>
#include <SFML/System.hpp>
#include <optional>

class World;
class Entity;
struct TransformComponent;

[[deprecated]] extern Entity& entityWithID(UUID id, World& world);
[[deprecated]] extern bool doesEntityExist(UUID id, World& world);

int positionToChunk(sf::Vector2<double> position);

bool isInRange(TransformComponent& player, TransformComponent& target, float range);
bool isBlockInRange(TransformComponent& player, sf::Vector2i& block, float range);

float getTickStep(uint16_t tick_rate);

constexpr unsigned int WORLD_UNIT_SIZE_FACTOR = 12;
constexpr uint8_t WORLD_TICKS_PER_SECOND = 60;

#endif // GAME_COMMON_HPP