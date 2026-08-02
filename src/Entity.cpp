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
#include "../include/TransformComponent.hpp"

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

Tag Entity::serialize() const
{
    TagCompound compound;

    for (const auto& [type_index, component] : components)
    {
        compound[component->name()] = component->serialize();
    }

    return Tag(compound);
}

void Entity::deserialize(Tag& tag)
{
    for(auto& [name, payload] : tag.get<TagCompound>())
    {
        if (name == "PhysicsComponent")
        {
            PhysicsComponent c;
            c.deserialize(payload);
            addComponent<PhysicsComponent>(c);
        }
        else if (name == "RenderComponent")
        {
            RenderComponent c;
            c.deserialize(payload);
            addComponent<RenderComponent>(c);
        }
        else if (name == "AnimationComponent")
        {
            AnimationComponent c;
            c.deserialize(payload);
            addComponent<AnimationComponent>(c);
        }
        else if (name == "InventoryComponent")
        {
            InventoryComponent c(1);
            c.deserialize(payload);
            addComponent<InventoryComponent>(c);
        }
        else if (name == "HealthComponent")
        {
            HealthComponent c;
            c.deserialize(payload);
            addComponent<HealthComponent>(c);
        }
        else if (name == "ItemComponent")
        {
            ItemComponent c;
            c.deserialize(payload);
            addComponent<ItemComponent>(c);
        }
        else if (name == "ExplosiveComponent")
        {
            ExplosiveComponent c;
            c.deserialize(payload);
            addComponent<ExplosiveComponent>(c);
        }
        else if (name == "TransformComponent")
        {
            TransformComponent c;
            c.deserialize(payload);
            addComponent<TransformComponent>(c);
        }
        else if (name == "AIComponent")
        {
            AIComponent c;
            c.deserialize(payload);
            addComponent<AIComponent>(c);
        }
        else if (name == "PlayerControlledComponent")
        {
            PlayerControlledComponent c;
            c.deserialize(payload);
            addComponent<PlayerControlledComponent>(c);
        }
    }
}