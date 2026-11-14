#ifndef PREFAB_HPP
#define PREFAB_HPP

#include "Entity.hpp"
#include "Item.hpp"
#include "Block.hpp"

#include <SFML/System/Vector2.hpp>

class World;

enum class PrefabID : uint8_t
{
    Player,
    Pig,
    Dynamite,
    DroppedItem,
    FallingBlock,
    Cube
};

// Thin adapter over the pack-populated gameRegistries().prefabs map. The
// enum-based API is still used across the codebase (ServerWorld::spawn,
// GameServer player spawn, structure editor, debug commands); internally it
// resolves to "blockbit:<name>" and looks the template up in the live
// registry that the pack loader fills in.
namespace PrefabRegistry
{
    const Entity& get(PrefabID prefab_id);
}

sf::Vector2f randomDropSpread(float spread_factor);

UUID spawnDroppedItem(World& world, sf::Vector2<double> position, const ItemStack& stack, sf::Vector2f force = {0.0f, 0.0f});

UUID spawnFallingBlock(World& world, sf::Vector2<double> position, BlockID block_id, uint16_t metadata = 0);

#endif // PREFAB_HPP