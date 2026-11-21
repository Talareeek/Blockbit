#include "../include/Entity.hpp"
#include "../include/Component.hpp"
#include "../include/PreserveComponent.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/ExplosiveComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/AIComponent.hpp"
#include "../include/PlayerControlledComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/StaminaComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/FallingBlockComponent.hpp"
#include "../include/LightningBoltComponent.hpp"
#include "../include/EffectsComponent.hpp"

Entity::Entity(UUID id) : id{id}
{

}

UUID Entity::getID() const
{
    return id;
}

std::unordered_map<std::type_index, std::unique_ptr<Component>>& Entity::getComponents()
{
    return components;
}

const std::unordered_map<std::type_index, std::unique_ptr<Component>>& Entity::getComponents() const
{
    return components;
}

Tag Entity::serializeForStorage() const
{
    TagCompound compound;

    for (const auto& [type_index, component] : components)
    {
        compound[component->name()] = component->serializeForStorage();
    }

    return Tag(compound);
}

Tag Entity::serializeForClientInit() const
{
    TagCompound compound;

    for (const auto& [type_index, component] : components)
    {
        if (component->side() == ComponentSide::Server) continue;
        compound[component->name()] = component->serializeForTransport();
    }

    return Tag(compound);
}

Tag Entity::serializeForClientUpdate() const
{
    TagCompound compound;

    for (const auto& [type_index, component] : components)
    {
        if (component->side() == ComponentSide::Server) continue;
        if (!component->dirty) continue;
        compound[component->name()] = component->serializeForTransport();
    }

    return Tag(compound);
}

void Entity::applyUpdate(const Tag& tag)
{
    const TagCompound& compound = tag.get<TagCompound>();

    for (auto& [type_index, component] : components)
    {
        auto it = compound.find(component->name());
        if (it == compound.end()) continue;

        component->deserializeFromTransport(it->second);
    }
}

void Entity::clearDirty()
{
    for (auto& [type_index, component] : components)
    {
        component->dirty = false;
    }
}

void Entity::deserializeFromStorage(Tag& tag)
{
    for(auto& [name, payload] : tag.get<TagCompound>())
    {
        if (name == "PhysicsComponent")
        {
            PhysicsComponent c;
            c.deserializeFromStorage(payload);
            addComponent<PhysicsComponent>(c);
        }
        else if (name == "RenderComponent")
        {
            RenderComponent c;
            c.deserializeFromStorage(payload);
            addComponent<RenderComponent>(c);
        }
        else if (name == "AnimationComponent")
        {
            AnimationComponent c;
            c.deserializeFromStorage(payload);
            addComponent<AnimationComponent>(c);
        }
        else if (name == "InventoryComponent")
        {
            InventoryComponent c(1);
            c.deserializeFromStorage(payload);
            addComponent<InventoryComponent>(c);
        }
        else if (name == "HealthComponent")
        {
            HealthComponent c;
            c.deserializeFromStorage(payload);
            addComponent<HealthComponent>(c);
        }
        else if (name == "StaminaComponent")
        {
            StaminaComponent c;
            c.deserializeFromStorage(payload);
            addComponent<StaminaComponent>(c);
        }
        else if (name == "ItemComponent")
        {
            ItemComponent c;
            c.deserializeFromStorage(payload);
            addComponent<ItemComponent>(c);
        }
        else if (name == "ExplosiveComponent")
        {
            ExplosiveComponent c;
            c.deserializeFromStorage(payload);
            addComponent<ExplosiveComponent>(c);
        }
        else if (name == "TransformComponent")
        {
            TransformComponent c;
            c.deserializeFromStorage(payload);
            addComponent<TransformComponent>(c);
        }
        else if (name == "AIComponent")
        {
            AIComponent c;
            c.deserializeFromStorage(payload);
            addComponent<AIComponent>(c);
        }
        else if (name == "PlayerControlledComponent")
        {
            PlayerControlledComponent c;
            c.deserializeFromStorage(payload);
            addComponent<PlayerControlledComponent>(std::move(c));
        }
        else if (name == "FallingBlockComponent")
        {
            FallingBlockComponent c;
            c.deserializeFromStorage(payload);
            addComponent<FallingBlockComponent>(c);
        }
        else if (name == "LightningBoltComponent")
        {
            LightningBoltComponent c;
            c.deserializeFromStorage(payload);
            addComponent<LightningBoltComponent>(c);
        }
        else if (name == "EffectsComponent")
        {
            EffectsComponent c;
            c.deserializeFromStorage(payload);
            addComponent<EffectsComponent>(c);
        }
    }
}

void Entity::deserializeFromClientInit(Tag& tag)
{
    for(auto& [name, payload] : tag.get<TagCompound>())
    {
        if (name == "PhysicsComponent")
        {
            PhysicsComponent c;
            c.deserializeFromTransport(payload);
            addComponent<PhysicsComponent>(c);
        }
        else if (name == "RenderComponent")
        {
            RenderComponent c;
            c.deserializeFromTransport(payload);
            addComponent<RenderComponent>(c);
        }
        else if (name == "AnimationComponent")
        {
            AnimationComponent c;
            c.deserializeFromTransport(payload);
            addComponent<AnimationComponent>(c);
        }
        else if (name == "InventoryComponent")
        {
            InventoryComponent c(1);
            c.deserializeFromTransport(payload);
            addComponent<InventoryComponent>(c);
        }
        else if (name == "HealthComponent")
        {
            HealthComponent c;
            c.deserializeFromTransport(payload);
            addComponent<HealthComponent>(c);
        }
        else if (name == "StaminaComponent")
        {
            StaminaComponent c;
            c.deserializeFromTransport(payload);
            addComponent<StaminaComponent>(c);
        }
        else if (name == "ItemComponent")
        {
            ItemComponent c;
            c.deserializeFromTransport(payload);
            addComponent<ItemComponent>(c);
        }
        else if (name == "ExplosiveComponent")
        {
            ExplosiveComponent c;
            c.deserializeFromTransport(payload);
            addComponent<ExplosiveComponent>(c);
        }
        else if (name == "TransformComponent")
        {
            TransformComponent c;
            c.deserializeFromTransport(payload);
            addComponent<TransformComponent>(c);
        }
        else if (name == "AIComponent")
        {
            AIComponent c;
            c.deserializeFromTransport(payload);
            addComponent<AIComponent>(c);
        }
        else if (name == "PlayerControlledComponent")
        {
            PlayerControlledComponent c;
            c.deserializeFromTransport(payload);
            addComponent<PlayerControlledComponent>(std::move(c));
        }
        else if (name == "FallingBlockComponent")
        {
            FallingBlockComponent c;
            c.deserializeFromTransport(payload);
            addComponent<FallingBlockComponent>(c);
        }
        else if (name == "LightningBoltComponent")
        {
            LightningBoltComponent c;
            c.deserializeFromTransport(payload);
            addComponent<LightningBoltComponent>(c);
        }
        else if (name == "EffectsComponent")
        {
            EffectsComponent c;
            c.deserializeFromTransport(payload);
            addComponent<EffectsComponent>(c);
        }
    }
}