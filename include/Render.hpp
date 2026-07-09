#ifndef RENDER_HPP
#define RENDER_HPP

#include "World.hpp"
#include <SFML/Graphics.hpp>

extern void RenderWorld(World& world, const sf::Vector2<double> camera, sf::RenderWindow& window);

extern void RenderBlockOverlay(World& world, const sf::Vector2<double> camera, sf::RenderWindow& window, uint32_t viewerEntityId);

extern void RenderLightRays(World& world, sf::RenderWindow& window);

extern sf::Color lerpColor(sf::Color a, sf::Color b, float t);

extern sf::Texture generateBackground();

#endif // RENDER_HPP