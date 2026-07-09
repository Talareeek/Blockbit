#ifndef RENDER_HPP
#define RENDER_HPP

#include "World.hpp"
#include "ChunkMesh.hpp"

#include <SFML/Graphics.hpp>
#include <unordered_map>

extern std::unordered_map<int, ChunkMesh> chunkMeshes;

extern void rebuildChunkMesh(World& world, int chunk_position, unsigned int unit_size);

extern void RenderWorld(World& world, const sf::Vector2<double> camera, sf::RenderWindow& window);

extern void RenderBlockOverlay(World& world, const sf::Vector2<double> camera, sf::RenderWindow& window, uint32_t viewerEntityId);

extern void RenderLightRays(World& world, sf::RenderWindow& window);

extern sf::Color lerpColor(sf::Color a, sf::Color b, float t);

extern sf::Texture generateBackground();

extern std::pair<sf::Color, sf::Color> getSkyGradient(float t);

extern void renderSky(sf::RenderWindow& window, sf::Color top, sf::Color bottom);

extern void renderSunAndMoon(float daytime, sf::RenderWindow& window);

extern sf::Vector2i getMouseBlockPosition(const World& world, const sf::RenderWindow& window);

extern sf::Vector2f getMouseWorldPosition(const World& world, const sf::RenderWindow& window);

extern sf::Vector2f getSunWorldPosition(const World& world, sf::Vector2f cameraCenter);

#endif // RENDER_HPP
