#include "../include/Prefab.hpp"
#include "../include/ContentProfiles.hpp"
#include "../include/GameRegistries.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/FallingBlockComponent.hpp"
#include "../include/World.hpp"

#include <iostream>
#include <random>
#include <stdexcept>

const Entity& PrefabRegistry::get(PrefabID prefab_id)
{
    // Vanilla prefabs live in the pack now (resources/packs/blockbit/prefabs/).
    // Look them up by their "blockbit:<name>" key in the shared registry that
    // the pack loader populates.
    const std::string key = "blockbit:" + ContentProfiles::nameFromPrefab(prefab_id);
    const auto& prefabs = gameRegistries().prefabs;
    auto it = prefabs.find(key);
    if (it == prefabs.end())
    {
        std::cerr << "[Prefab] missing vanilla prefab '" << key
                  << "' — did resources/packs/blockbit/prefabs/" << key.substr(9)
                  << ".json load?\n";
        throw std::runtime_error("PrefabRegistry::get: prefab '" + key + "' not registered");
    }
    return it->second;
}

sf::Vector2f randomDropSpread(float spread_factor)
{
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> x_dist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> y_dist(0.0f, 1.0f);

    return {x_dist(rng) * spread_factor, y_dist(rng) * spread_factor};
}

UUID spawnDroppedItem(World& world, sf::Vector2<double> position, const ItemStack& stack, sf::Vector2f force)
{
    Entity entity = PrefabRegistry::get(PrefabID::DroppedItem).cloneAs(generateUUID());

    auto& transform = entity.getComponent<TransformComponent>();
    transform.position = position;
    transform.previous_position = position;
    transform.render_position = position;

    entity.getComponent<PhysicsComponent>().force = sf::Vector2<double>(force.x, force.y);
    entity.getComponent<ItemComponent>().item = stack;
    entity.getComponent<RenderComponent>().textureID = itemDatabase[stack.itemID].texture;

    UUID id = entity.getID();
    world.addEntity(std::move(entity));
    return id;
}

UUID spawnFallingBlock(World& world, sf::Vector2<double> position, BlockID block_id, uint16_t metadata)
{
    Entity entity = PrefabRegistry::get(PrefabID::FallingBlock).cloneAs(generateUUID());

    auto& transform = entity.getComponent<TransformComponent>();
    transform.position = position;
    transform.previous_position = position;
    transform.render_position = position;

    auto& render = entity.getComponent<RenderComponent>();
    const auto db_it = blockDatabase.find(block_id);
    if (db_it != blockDatabase.end() && db_it->second.render.has_value())
    {
        render.textureID = db_it->second.render->texture;
    }

    entity.getComponent<FallingBlockComponent>() = FallingBlockComponent{block_id, metadata};

    UUID id = entity.getID();
    world.addEntity(std::move(entity));
    return id;
}