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

size_t Entity::getIndexInChunk() const
{
    return index_in_chunk;
}

int Entity::getChunk() const
{
    return chunk;
}

void Entity::setIndexInChunk(size_t index)
{
    index_in_chunk = index;
}

void Entity::setChunk(int chunk_position)
{
    chunk = chunk_position;
}


bool Entity::getAssignedChunk() const
{
    return assigned_chunk;
}

void Entity::setAssignedChunk(bool assigned)
{
    assigned_chunk = assigned;
}