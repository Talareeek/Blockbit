#ifndef ENTITY_HPP
#define ENTITY_HPP

#include <unordered_map>
#include <typeindex>
#include <cstdint>
#include <any>
#include <exception>

class Entity
{
private:

    uint32_t id;

    std::unordered_map<std::type_index, std::any> components;

public:

    Entity(uint32_t id);

    uint32_t getID() const;

    std::unordered_map<std::type_index, std::any>& getComponents();

    const std::unordered_map<std::type_index, std::any>& getComponents() const;

    template<typename T>
    void addComponent(T component)
    {
        components[typeid(T)] = std::move(component);
    }

    template<typename T>
    bool hasComponent() const 
    {
        return components.contains(typeid(T));
    }

    template<typename T>
    T& getComponent()
    {
        return std::any_cast<T&>(components.at(typeid(T)));
    }
};

#endif // ENTITY_HPP