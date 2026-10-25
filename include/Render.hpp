#ifndef RENDER_HPP
#define RENDER_HPP

#include "World.hpp"
#include "ClientWorld.hpp"

#include <SFML/Graphics.hpp>

extern void BuildChunkMesh(ClientWorld& world, int chunk_position);

extern void RenderWorld(ClientWorld& world, const sf::Vector2<double> camera, sf::RenderWindow& window);

[[deprecated]] extern void RenderBlockOverlay(World& world, const sf::Vector2<double> camera, sf::RenderWindow& window, UUID viewerEntityId);

extern void RenderBlockOutline(World& world, const sf::Vector2<double> camera, const sf::Vector2i block, sf::RenderWindow& window);

extern void RenderBlockDestruction(World& world, sf::Vector2<double> camera, sf::RenderWindow& window);

extern void RenderLightRays(World& world, sf::RenderWindow& window);

extern sf::Color lerpColor(sf::Color a, sf::Color b, float t);

extern sf::Texture generateBackground();

extern std::pair<sf::Color, sf::Color> getSkyGradient(float t);

extern void renderSky(sf::RenderTarget& target, sf::Color top, sf::Color bottom);

// phase is a normalized sun phase in [0, 1): 0=midnight, 0.25=sunrise,
// 0.5=noon, 0.75=sunset. Compute with GameCommon::solarPhase so sunrise /
// sunset slide with the season.
extern void renderSunAndMoon(float phase, sf::RenderWindow& window);

extern void renderStars(float phase, sf::RenderWindow& window);

extern sf::Vector2i getMouseBlockPosition(sf::Vector2<double> camera, const sf::RenderWindow& window);

extern sf::Vector2<double> getMouseWorldPosition(sf::Vector2<double> camera, const sf::RenderWindow& window);

extern sf::Vector2f getSunWorldPosition(const World& world, sf::Vector2f cameraCenter);

extern void renderUIBackground(sf::FloatRect bounds, sf::RenderTarget& target);

// render_tick is the fractional server-tick index the client wants to display.
// Compute it as (latest_tick + time_since_snapshot/tick_step) - interpolation_delay_ticks
// so the view interpolates continuously between snapshots instead of only
// stepping forward when a new snapshot arrives.
extern void NetworkInterpolationSystem(World& world, double render_tick);

extern void renderItemInfo(sf::Vector2f position, const ItemStack& item, sf::RenderTarget& target);

extern sf::Color getRarityColor(ItemRarity rarity);

extern void renderBar(int max, int value, sf::Color primary_color, sf::Color secondary_color, sf::FloatRect bounds, sf::RenderTarget& target);

extern void renderHotbar(std::array<ItemStack, 9> items, uint8_t selected_slot, sf::RenderTarget& target);

extern void RenderMinimap(ClientWorld& world, sf::Vector2<double> camera, sf::RenderTarget& target);

#endif // RENDER_HPP
