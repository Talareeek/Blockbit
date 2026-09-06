#ifndef RENDER_HPP
#define RENDER_HPP

#include "World.hpp"
#include "ChunkMesh.hpp"

#include <SFML/Graphics.hpp>
#include <unordered_map>

extern std::unordered_map<int, ChunkMesh> chunkMeshes;

extern void rebuildChunkMesh(World& world, int chunk_position, float unit_size);

extern void RenderWorld(World& world, const sf::Vector2<double> camera, sf::RenderWindow& window);

[[deprecated]] extern void RenderBlockOverlay(World& world, const sf::Vector2<double> camera, sf::RenderWindow& window, UUID viewerEntityId);

extern void RenderBlockOutline(const sf::Vector2<double> camera, const sf::Vector2i block, sf::RenderWindow& window);

extern void RenderLightRays(World& world, sf::RenderWindow& window);

extern sf::Color lerpColor(sf::Color a, sf::Color b, float t);

extern sf::Texture generateBackground();

extern std::pair<sf::Color, sf::Color> getSkyGradient(float t);

extern void renderSky(sf::RenderWindow& window, sf::Color top, sf::Color bottom);

extern void renderSunAndMoon(float daytime, sf::RenderWindow& window);

extern void renderStars(float daytime, sf::RenderWindow& window);

extern sf::Vector2i getMouseBlockPosition(sf::Vector2<double> camera, const sf::RenderWindow& window);

extern sf::Vector2<double> getMouseWorldPosition(sf::Vector2<double> camera, const sf::RenderWindow& window);

extern sf::Vector2f getSunWorldPosition(const World& world, sf::Vector2f cameraCenter);

extern void renderUIBackground(sf::FloatRect bounds, sf::RenderTarget& target);

extern void NetworkInterpolationSystem(World& world, uint64_t latest_tick, float tick_step, float interpolation_delay_seconds = 0.1f);

extern void renderItemInfo(sf::Vector2f position, const ItemStack& item, sf::RenderTarget& target);

extern sf::Color getRarityColor(ItemRarity rarity);

extern void renderBar(int max, int value, sf::Color primary_color, sf::Color secondary_color, sf::FloatRect bounds, sf::RenderTarget& target);

#endif // RENDER_HPP
