#include "../include/TransformComponent.hpp"

void TransformComponent::teleport(const sf::Vector2<double>& position)
{
    this->position = position;
    render_position = position;
}

void TransformComponent::move(const sf::Vector2<double>& offset)
{
    position += offset;
    render_position = position;
}