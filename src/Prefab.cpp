#include "../include/Prefab.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/ExplosiveComponent.hpp"
#include "../include/AIComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/World.hpp"

#include <random>

std::unordered_map<PrefabID, Entity> PrefabRegistry::prefabs;

void PrefabRegistry::add(PrefabID prefab_id, Entity& entity)
{
    prefabs.emplace(prefab_id, std::move(entity));
}

const Entity& PrefabRegistry::get(PrefabID prefab_id)
{
    return prefabs.at(prefab_id);
}

void PrefabRegistry::init()
{
    { // PLAYER
        Entity entity(UUID{});

        entity.addComponent(TransformComponent{{0.0f, 0.0f}, {0.75f, 1.0f}, sf::degrees(0.0f)});
        entity.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});
        entity.addComponent(HealthComponent{100, 100, true});

        InventoryComponent inv(36);
        entity.addComponent(std::move(inv));

        entity.addComponent(RenderComponent{AssetManager::GameTextureID::Player, {{0, 0}, {12, 16}}, {0.75f, 1.0f}});

        AnimationComponent animation;

        animation.animations =
        {
            {AnimationState::Idle, AnimationClip{0, 2, 0.5f, true}},
            {AnimationState::Walking, AnimationClip{2, 2, 0.5f, true}}
        };

        animation.frameSize = {12, 16};

        entity.addComponent(animation);

        add(PrefabID::Player, entity);
    }

    { // DYNAMITE
        Entity entity(generateUUID());

        entity.addComponent(TransformComponent{{0.0, 0.0}, {1.0, 1.0}, sf::degrees(0.0f)});

        entity.addComponent(ExplosiveComponent{3.0f});

        entity.addComponent(RenderComponent{AssetManager::GameTextureID::Dynamite, {{0, 0}, {16, 16}}, {1.0f, 1.0f}});

                
        entity.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});

        add(PrefabID::Dynamite, entity);
    }

    { // PIG
        Entity entity(generateUUID());

        entity.addComponent(TransformComponent{{0.0, 0.0}, {1.0f, 0.75f}, sf::degrees(0.0f)});
        entity.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});
        entity.addComponent(RenderComponent{AssetManager::GameTextureID::Pig, {{0, 0}, {16, 12}}, {1.0f, 0.75f}});
        entity.addComponent(HealthComponent{100, 100, true});

        AIComponent ai;
        ai.personality = AIComponent::Personality::Passive;
        ai.state       = AIComponent::State::Idle;
        entity.addComponent(ai);

        AnimationComponent animation;

        animation.animations =
        {
            {AnimationState::Idle, AnimationClip{0, 2, 0.5f, true}},
            {AnimationState::Walking, AnimationClip{2, 2, 0.5f, true}}
        };

        animation.frameSize = {16, 12};

        entity.addComponent(animation);

        add(PrefabID::Pig, entity);
    }

    { // DROPPED_ITEM
        Entity entity(UUID{});

        entity.addComponent(TransformComponent{{0.0, 0.0}, {0.5, 0.5}, sf::degrees(0.0f)});
        entity.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, false, false, false, true});
        entity.addComponent(RenderComponent{static_cast<AssetManager::GameTextureID>(0), {{0, 0}, {16, 16}}, {0.5f, 0.5f}});
        entity.addComponent(ItemComponent{});

        add(PrefabID::DroppedItem, entity);
    }
};

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

    entity.getComponent<PhysicsComponent>().force = force;
    entity.getComponent<ItemComponent>().item = stack;
    entity.getComponent<RenderComponent>().textureID = itemDatabase[stack.itemID].texture;

    UUID id = entity.getID();
    world.addEntity(std::move(entity));
    return id;
}