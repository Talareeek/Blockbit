#include "../include/ItemBehavior.hpp"

#include <cmath>

#include <algorithm>

#include <iostream>

#include "../include/AssetManager.hpp"
#include "../include/Behavior.hpp"
#include "../include/Block.hpp"
#include "../include/BlockStates.hpp"
#include "../include/Chunk.hpp"
#include "../include/EffectsComponent.hpp"
#include "../include/Entity.hpp"
#include "../include/ExplosiveComponent.hpp"
#include "../include/FurnaceBlockEntity.hpp"
#include "../include/GameRegistries.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/Item.hpp"
#include "../include/ItemDataComponents.hpp"
#include "../include/ItemInstance.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/World.hpp"

namespace
{

// Clones the prefab referenced by `prefab_key` and injects the given spawn
// position + velocity. Everything else (render / explosive / physics flags
// / health / AI …) comes from the prefab template — so changing a JSON's
// "projectile" value actually changes what gets thrown.
bool spawnFromPrefab(World& world,
                     const std::string& prefab_key,
                     sf::Vector2<double> position,
                     sf::Vector2<double> velocity)
{
    const auto& prefabs = gameRegistries().prefabs;
    auto it = prefabs.find(prefab_key);
    if (it == prefabs.end())
    {
        std::cerr << "[Item] throw_projectile: unknown prefab '" << prefab_key << "'\n";
        return false;
    }

    Entity entity = it->second.cloneAs(generateUUID());

    if (entity.hasComponent<TransformComponent>())
        entity.getComponent<TransformComponent>().position = position;

    if (entity.hasComponent<PhysicsComponent>())
        entity.getComponent<PhysicsComponent>().velocity = velocity;

    world.addEntity(std::move(entity));
    return true;
}

bool handleThrowProjectile(const ThrowProjectileParams& p,
                           World& world,
                           sf::Vector2<double> mouse,
                           UUID user)
{
    using Pattern = ThrowProjectileParams::Pattern;

    switch (p.pattern)
    {
        case Pattern::Single:
        {
            // Spawn at player, throw toward mouse with linear scale.
            Entity& player = world.getEntity(user);
            if (!player.hasComponent<TransformComponent>()) return false;

            const sf::Vector2<double> origin =
                player.getComponent<TransformComponent>().position;

            return spawnFromPrefab(
                world, p.projectile, origin,
                (mouse - origin) * static_cast<double>(p.throw_speed));
        }

        case Pattern::Column:
        {
            const int column_x = static_cast<int>(std::floor(mouse.x));
            const uint32_t height = (p.count == 0) ? 1 : p.count;
            for (uint32_t y = 1; y <= height; ++y)
            {
                spawnFromPrefab(
                    world, p.projectile,
                    {static_cast<double>(column_x) + 0.5,
                     static_cast<double>(y) + 0.5},
                    {0.0, 0.0});
            }
            return true;
        }

        case Pattern::Arc:
        {
            const sf::Vector2<double> spawn_pos = {
                std::floor(mouse.x) + 0.5,
                std::floor(mouse.y) + 0.5 + static_cast<double>(p.height_offset)
            };
            const uint32_t count = (p.count == 0) ? 1 : p.count;
            for (uint32_t i = 0; i < count; ++i)
            {
                const float t = (count == 1)
                    ? 0.f
                    : (static_cast<float>(i) / static_cast<float>(count - 1)) * 2.f - 1.f;
                const double vx = static_cast<double>(t) * static_cast<double>(p.spread);
                spawnFromPrefab(world, p.projectile, spawn_pos, {vx, 0.0});
            }
            return true;
        }
    }
    return false;
}

bool handleFluidBucket(const FluidBucketParams& p,
                       World& world,
                       sf::Vector2<double> mouse,
                       UUID user,
                       const ItemStack& stack)
{
    Entity& player = world.getEntity(user);
    if (!player.hasComponent<InventoryComponent>()) return false;

    auto& inventory = player.getComponent<InventoryComponent>().inventory;

    const sf::Vector2i block_pos = {
        static_cast<int>(std::floor(mouse.x)),
        static_cast<int>(std::floor(mouse.y))
    };

    const Block target = world.getBlock(block_pos.x, block_pos.y);

    // Empty bucket: pick up a source cell and swap to the filled variant.
    if (stack.itemID == p.empty_variant)
    {
        if (target.id != p.fluid) return false;
        if (WaterState::unpack(target.metadata).level != p.source_level_value) return false;

        world.setBlock(block_pos.x, block_pos.y, {BlockID::Air, 0});
        inventory.removeItemWithLeftover(p.empty_variant, 1);
        inventory.addItemWithLeftover(p.filled_variant, 1);
        return false;
    }

    // Filled bucket: deposit into air or partial-fluid, swap back to empty.
    if (stack.itemID == p.filled_variant)
    {
        const bool is_air          = (target.id == BlockID::Air);
        const bool is_partial_fluid = (target.id == p.fluid)
            && (WaterState::unpack(target.metadata).level < p.source_level_value);
        if (!is_air && !is_partial_fluid) return false;

        world.setBlock(block_pos.x, block_pos.y,
                       {p.fluid, WaterState{p.source_level_value}.pack()});
        inventory.removeItemWithLeftover(p.filled_variant, 1);
        inventory.addItemWithLeftover(p.empty_variant, 1);
        return false;
    }

    return false;
}

// Attempts to light a furnace standing at `position`. Returns true if the
// furnace was lit; false means the cell wasn't a furnace, or the furnace
// was already lit, or fuel/input/recipes didn't line up.
bool tryIgniteFurnace(World& world, sf::Vector2i position)
{
    Block target = world.getBlock(position.x, position.y);
    if (target.id != BlockID::Furnace) return false;

    auto* furnace = dynamic_cast<FurnaceBlockEntity*>(
        world.getBlockEntity(position.x, position.y));
    if (!furnace) return false;
    if (furnace->isLit()) return false;
    if (furnace->fuel.empty() || furnace->input.empty()) return false;

    auto fuel_it = fuelBurnTimes.find(furnace->fuel.itemID);
    if (fuel_it == fuelBurnTimes.end()) return false;

    auto smelt_it = smeltingRecipes.find(furnace->input.itemID);
    if (smelt_it == smeltingRecipes.end()) return false;

    furnace->lit                 = true;
    furnace->burn_time_total     = fuel_it->second;
    furnace->burn_time_remaining = fuel_it->second;
    furnace->cook_time_total     = smelt_it->second.cook_time;
    furnace->cook_time_remaining = smelt_it->second.cook_time;

    if (--furnace->fuel.quantity == 0) furnace->fuel = {ItemID::None, 0};

    if (!FurnaceState::unpack(target.metadata).lit)
    {
        world.setBlock(position.x, position.y,
                       {BlockID::Furnace, FurnaceState{true}.pack()});
    }
    return true;
}

bool tryPlaceFire(World& world, sf::Vector2i position)
{
    const Block target = world.getBlock(position.x, position.y);
    if (target.id != BlockID::Air) return false;

    const Block below = world.getBlock(position.x, position.y - 1);
    if (!blockDatabase[below.id].solid) return false;

    world.setBlock(position.x, position.y, {BlockID::Fire, 0});
    return true;
}

bool handleIgnite(const IgniteParams& p,
                  World& world,
                  sf::Vector2<double> mouse)
{
    const sf::Vector2i position = {
        static_cast<int>(std::floor(mouse.x)),
        static_cast<int>(std::floor(mouse.y))
    };

    if (tryIgniteFurnace(world, position))
        return p.consume_on_success;

    if (p.can_place_fire && tryPlaceFire(world, position))
        return p.consume_on_success;

    return false;
}

bool handleConsumeEffect(World& world, UUID user, const ItemStack& stack)
{
    if (!stack.instanceID.has_value()) return false;

    const ItemInstanceData* data = findInstance(*stack.instanceID);
    if (!data) return false;

    const MagicEffectItemComponent* bottle = data->get<MagicEffectItemComponent>();
    if (!bottle || bottle->effect == EffectID::None || bottle->duration <= 0.0f)
        return false;

    Entity& player = world.getEntity(user);
    if (!player.hasComponent<EffectsComponent>()) return false;

    auto& effects = player.getComponent<EffectsComponent>();
    EffectInstance* existing = nullptr;
    for (auto& e : effects.effects)
    {
        if (e.id == bottle->effect) { existing = &e; break; }
    }

    if (existing)
    {
        existing->remaining_duration = std::max(existing->remaining_duration, bottle->duration);
        existing->magnitude          = std::max(existing->magnitude, bottle->magnitude);
    }
    else
    {
        EffectInstance e;
        e.id                 = bottle->effect;
        e.interval           = 999.0f;
        e.remaining_duration = bottle->duration;
        e.magnitude          = bottle->magnitude;
        effects.effects.push_back(e);
    }
    effects.dirty = true;
    return true;
}

} // namespace

bool dispatchItemUse(const ItemBehavior& behavior,
                     World& world,
                     sf::Vector2<double> mouse,
                     UUID user,
                     const ItemStack& stack)
{
    switch (behavior.type)
    {
        case ItemBehaviorType::None:
            return false;

        case ItemBehaviorType::ThrowProjectile:
            if (const auto* p = std::get_if<ThrowProjectileParams>(&behavior.params))
                return handleThrowProjectile(*p, world, mouse, user);
            return false;

        case ItemBehaviorType::FluidBucket:
            if (const auto* p = std::get_if<FluidBucketParams>(&behavior.params))
                return handleFluidBucket(*p, world, mouse, user, stack);
            return false;

        case ItemBehaviorType::Ignite:
            if (const auto* p = std::get_if<IgniteParams>(&behavior.params))
                return handleIgnite(*p, world, mouse);
            return false;

        case ItemBehaviorType::ConsumeEffect:
            return handleConsumeEffect(world, user, stack);
    }
    return false;
}
