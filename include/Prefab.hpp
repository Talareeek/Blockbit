#ifndef PREFAB_HPP
#define PREFAB_HPP

#include "Entity.hpp"
#include "Item.hpp"
#include "Block.hpp"

#include <SFML/System/Vector2.hpp>

class World;

enum class PrefabID
{
    Player,
    Pig,
    Dynamite,
    DroppedItem,
    FallingBlock
};

class PrefabRegistry
{
private:

    static std::unordered_map<PrefabID, Entity> prefabs;

public:

    static void add(PrefabID prefab_id, Entity& entity);
    static const Entity& get(PrefabID prefab_id);

    static void init();
};

sf::Vector2f randomDropSpread(float spread_factor);

UUID spawnDroppedItem(World& world, sf::Vector2<double> position, const ItemStack& stack, sf::Vector2f force = {0.0f, 0.0f});

UUID spawnFallingBlock(World& world, sf::Vector2<double> position, BlockID block_id, uint8_t metadata = 0);

#endif // PREFAB_HPP