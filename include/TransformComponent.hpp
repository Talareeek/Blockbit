#ifndef TRANSFORM_COMPONENT_HPP
#define TRANSFORM_COMPONENT_HPP

#include <SFML/System/Vector2.hpp>

struct TransformComponent
{
    sf::Vector2f position;
    sf::Vector2f size;
    sf::Angle rotation;

    void teleport(const sf::Vector2f& position);
    void move(const sf::Vector2f& offset);
};

#endif // TRANSFORM_COMPONENT_HPP