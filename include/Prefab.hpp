#ifndef PREFAB_HPP
#define PREFAB_HPP

#include "Entity.hpp"
#include "Item.hpp"
#include "Block.hpp"

#include <SFML/System/Vector2.hpp>

class World;

// Prefabs are addressed by their full pack registry key.
namespace PrefabRegistry
{
    const Entity& get(const std::string& prefab_key);
}

sf::Vector2f randomDropSpread(float spread_factor);

UUID spawnDroppedItem(World& world, sf::Vector2<double> position, const ItemStack& stack, sf::Vector2f force = {0.0f, 0.0f});

UUID spawnFallingBlock(World& world, sf::Vector2<double> position, BlockKey block_id, uint16_t metadata = 0);

#endif // PREFAB_HPP
