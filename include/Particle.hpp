#ifndef PARTICLE_HPP
#define PARTICLE_HPP

#include <SFML/System/Vector2.hpp>
#include <cstdint>

struct Particle
{
    sf::Vector2f position;
    sf::Vector2f velocity;
    float lifetime;
    bool fading;

    uint32_t texture;
};

#endif // PARTICLE_HPP