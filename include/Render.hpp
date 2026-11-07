#ifndef RENDER_HPP
#define RENDER_HPP

#include "World.hpp"
#include "ClientWorld.hpp"

#include <SFML/Graphics.hpp>

extern void BuildChunkMesh(ClientWorld& world, int chunk_position);

extern void RenderWorld(ClientWorld& world, const sf::Vector2<double> camera, sf::RenderWindow& window);

[[deprecated]] extern void RenderBlockOverlay(World& world, const sf::Vector2<double> camera, sf::RenderWindow& window, UUID viewerEntityId);

extern void RenderBlockOutline(World& world, const sf::Vector2<double> camera, const sf::Vector2i block, UUID viewerEntityId, sf::RenderWindow& window);

extern void RenderEntityHitboxes(World& world, const sf::Vector2<double> camera, sf::RenderWindow& window);

extern void RenderChunkBorders(const sf::Vector2<double> camera, sf::RenderWindow& window);

extern void RenderBorder(const sf::Vector2<double> camera, float view_scale, sf::RenderWindow& window);

extern void RenderBlockDestruction(World& world, sf::Vector2<double> camera, sf::RenderWindow& window);

extern void RenderLightRays(World& world, sf::RenderWindow& window);

extern sf::Color lerpColor(sf::Color a, sf::Color b, float t);

extern sf::Texture generateBackground();

extern std::pair<sf::Color, sf::Color> getSkyGradient(float t);

// Same curve as getSkyGradient but tinted toward a stormy gray by
// rain_intensity, and darker still by thunder_intensity. Both are 0..1 and are
// smoothed on the client — a flat flag->color mapping would pop.
extern std::pair<sf::Color, sf::Color> getSkyGradientWeathered(float t, float rain_intensity, float thunder_intensity);

extern void renderSky(sf::RenderTarget& target, sf::Color top, sf::Color bottom);

// Draws batched rain/snow quads inside a cylinder around the camera. Reads
// chunks straight from world (via getBlock) to find the top opaque block per
// column so drops clip at roofs instead of falling through them. Biome of the
// column decides between rain and snow. intensity scalars gate both the drop
// count and alpha, so a 0 intensity costs almost nothing.
extern void renderPrecipitation(ClientWorld& world, sf::Vector2<double> camera,
                                float rain_intensity, float thunder_intensity,
                                sf::RenderWindow& window);

// Scans the world for entities carrying LightningBoltComponent and draws a
// procedural zigzag per strike. Alpha follows each bolt's remaining lifetime
// so it fades out before the server deletes the entity.
extern void renderLightningEntities(World& world, sf::Vector2<double> camera, sf::RenderWindow& window);

// Fullscreen white overlay used for a lightning strike flash. intensity is
// 0..1; decayed on the client. Called once per frame when the flash timer is
// above zero.
extern void renderLightningFlash(float intensity, sf::RenderTarget& target);

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
