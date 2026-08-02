#ifndef ENTITY_HPP
#define ENTITY_HPP

#include "UUID.hpp"
#include "BBT.hpp"
#include "Component.hpp"

#include <unordered_map>
#include <typeindex>
#include <cstdint>
#include <memory>
#include <exception>

class Entity
{
private:

    UUID id;

    std::unordered_map<std::type_index, std::unique_ptr<Component>> components;

public:

    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;
    Entity(Entity&&) = default;
    Entity& operator=(Entity&&) = default;

    Entity(UUID id);

    UUID getID() const;

    std::unordered_map<std::type_index, std::unique_ptr<Component>>& getComponents();

    const std::unordered_map<std::type_index, std::unique_ptr<Component>>& getComponents() const;

    template<typename T>
    void addComponent(T component)
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
        components[typeid(T)] = std::make_unique<T>(std::move(component));
    }

    template<typename T>
    bool hasComponent() const
    {
        return components.contains(typeid(T));
    }

    template<typename T>
    T& getComponent()
    {
        return static_cast<T&>(*components.at(typeid(T)));
    }

    template<typename T>
    const T& getComponent() const
    {
        return static_cast<const T&>(*components.at(typeid(T)));
    }

    Tag serialize() const;
    void deserialize(Tag& tag);
};

#endif // ENTITY_HPP