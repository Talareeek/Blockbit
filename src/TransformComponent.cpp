#include "../include/TransformComponent.hpp"

void TransformComponent::teleport(const sf::Vector2f& position)
{
    this->position = position;
}

void TransformComponent::move(const sf::Vector2f& offset)
{
    position += offset;
}