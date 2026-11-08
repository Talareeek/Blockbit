#ifndef PARTICLE_HPP
#define PARTICLE_HPP

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <cstdint>

struct Particle
{
    sf::Vector2<double> position;  // world coords (block units) — must be
                                   // double so particles far from origin
                                   // (close to BORDER_X) still animate at
                                   // sub-block precision
    sf::Vector2<double> velocity;  // blocks / second
    float        lifetime;         // seconds remaining
    float        max_lifetime;     // original lifetime, for fade ratio
    float        scale;            // sprite size in blocks

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
        // Particle is subject to world lighting — its RGB is multiplied by
        // the local block/sky light at render time. Leave off for self-
        // illuminating effects (flame, spark, ember, explosion) so they
        // stay bright at night.
        Lit      = 1 << 5,
    };
};

#endif // PARTICLE_HPP
