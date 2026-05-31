#ifndef RENDER_HPP
#define RENDER_HPP

#include "World.hpp"
#include <SFML/Graphics.hpp>

extern void RenderWorld(World& world, sf::RenderWindow& window);

extern void RenderBlockOverlay(World& world, sf::RenderWindow& window);

extern void RenderLightRays(World& world, sf::RenderWindow& window);

extern sf::Color lerpColor(sf::Color a, sf::Color b, float t);

#endif // RENDER_HPP