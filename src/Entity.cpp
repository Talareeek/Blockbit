#include "../include/Entity.hpp"

Entity::Entity(uint32_t id) : id{id}
{

}

uint32_t Entity::getID() const
{
    return id;
}

std::unordered_map<std::type_index, std::any>& Entity::getComponents()
{
    return components;
}

const std::unordered_map<std::type_index, std::any>& Entity::getComponents() const
{
    return components;
}