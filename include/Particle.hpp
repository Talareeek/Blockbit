#ifndef PARTICLE_HPP
#define PARTICLE_HPP

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <cstdint>

struct Particle
{
    sf::Vector2f position;        // world coords (block units)
    sf::Vector2f velocity;        // blocks / second
    float        lifetime;        // seconds remaining
    float        max_lifetime;    // original lifetime, for fade ratio
    float        scale;           // sprite size in blocks

    sf::Color    color;

    // Textured particles sample from this rect. nullptr → solid-color quad.
    // Block-dust particles plug the real block texture here so they look
    // like fragments of what was broken.
    const sf::Texture* texture = nullptr;
    sf::IntRect        texture_rect{};

    uint8_t flags = 0;

    enum Flag : uint8_t
    {
        Gravity  = 1 << 0,
        Collides = 1 << 1,
        Fades    = 1 << 2,
        Rises    = 1 << 3,
        Flicker  = 1 << 4,
    };
};

#endif // PARTICLE_HPP
