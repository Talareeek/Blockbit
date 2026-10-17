#include "../include/Prefab.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/ExplosiveComponent.hpp"
#include "../include/AIComponent.hpp"

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
        entity.addComponent(HealthComponent{100, 100});

        InventoryComponent inv(36);
        inv.inventory.slots[0] = {ItemID::Dynamite, 16};
        inv.inventory.slots[1] = {ItemID::Bucket, 1};
        inv.inventory.slots[2] = {ItemID::Woodcutter, 64};
        inv.inventory.slots[3] = {ItemID::Lighter, 1};
        inv.inventory.slots[4] = {ItemID::Diamond_Pickaxe, 1};
        inv.inventory.slots[5] = {ItemID::Diamond_Axe, 1};
        inv.inventory.slots[6] = {ItemID::Diamond_Shovel, 1};
        inv.inventory.slots[7] = {ItemID::Chest, 64};
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
        entity.addComponent(HealthComponent{100, 100});

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
};