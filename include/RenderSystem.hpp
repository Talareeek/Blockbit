#ifndef RENDER_SYSTEM_HPP
#define RENDER_SYSTEM_HPP

#include "Entity.hpp"

#include <SFML/Graphics.hpp>
#include <vector>

extern float worldToScreenY(float worldY, float unit_size, float windowHeight);

float screenToWorldY(float screenY, float unit_size, float windowHeight);

extern void RenderSystem(std::vector<Entity>& entities, sf::RenderWindow& window);

#endif // RENDER_SYSTEM_HPP