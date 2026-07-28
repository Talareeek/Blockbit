#include "../include/Entity.hpp"
#include "../include/Component.hpp"


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

Tag Entity::serialize() const
{
    TagCompound compound;

    for(auto& [type_index, component] : components)
    {
        compound[reinterpret_cast<const Component&>(component).name()] = reinterpret_cast<const Component&>(component).serialize();        
    }

    return Tag(compound);
}