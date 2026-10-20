#ifndef BLOCK_HPP
#define BLOCK_HPP

#include <cstdint>
#include <unordered_map>
#include <climits>
#include <optional>
#include "Item.hpp"
#include "AssetManager.hpp"

struct PostPlaceBlockUpdate
{
    sf::Vector2i target;
    sf::Vector2i sender;
};

enum class BlockID : uint32_t
{
    Air,
    Stone,
    Grass,
    Dirt,
    Cobblestone,
    Obsidian,
    Bedrock,
    Water,
    Coal_Ore,
    Iron_Ore,
    Gold_Ore,
    Diamond_Ore,
    Ruby_Ore,
    Iron_Block,
    Gold_Block,
    Ruby_Block,
    Oak_Log,
    Oak_Leaves,
    Oak_Planks,
    Woodcutter,
    Fire,
    Sand,
    Coarse_Dirt,
    Snow,
    Chest,
    Bricks,
    Furnace
};

struct Block
{
    BlockID id;
    uint8_t metadata;
};

struct MiningProperties
{
    int mining_resistance;

    ToolProperties desired_tool;
};

struct DropProperties
{
    bool always_drop = false;
    ItemID drop;

    uint8_t min_amount = 1;
    uint8_t max_amount = 1;
};

// Shader-side animation descriptor. frame_count == 1 means "no animation";
// Render.cpp's appendQuad packs these into vertex color so the chunk mesh
// stays static even while the GPU shifts U each frame.
struct BlockAnimation
{
    uint8_t  frame_count       = 1;
    uint16_t frame_duration_ms = 200;
};

struct RenderProperties
{
    AssetManager::GameTextureID texture;
    std::function<sf::FloatRect(Block block_instance)> render_bounds = [](Block block_instance) -> sf::FloatRect{return {{0.0f, 0.0f}, {0.0f, 0.0f}};};
    std::function<sf::IntRect(Block block_instance)> rect = [](Block block_instance) -> sf::IntRect{return {{0, 0}, {0, 0}};};
    std::function<BlockAnimation(Block block_instance)> animation = [](Block) -> BlockAnimation { return {}; };
};

struct BlockData
{
    bool solid;
    bool transparent;
    bool breakable;

    bool liquid = false;
    float drag = 1.0f;

    float hardness;

    std::optional<RenderProperties> render;
    std::optional<MiningProperties> mining;
    std::optional<DropProperties> drop;
    std::optional<std::function<void(PostPlaceBlockUpdate, World)>> update;
};

extern std::unordered_map<BlockID, BlockData> blockDatabase;

inline auto default_render_bounds = [](Block block_instance) -> sf::FloatRect
{
    return {{0.0f, 0.0f}, {1.0f, 1.0f}};
};

inline auto default_render_rect = [](Block block_instance) -> sf::IntRect
{
    auto data = blockDatabase[block_instance.id];

    return {{0, 0}, {static_cast<int>(AssetManager::getGameTexture(data.render->texture).getSize().x), static_cast<int>(AssetManager::getGameTexture(data.render->texture).getSize().y)}};
};

inline auto water_render_bounds = [](Block block_instance) -> sf::FloatRect
{
    return {{0.0f, 0.0f}, {1.0f, static_cast<float>(16 - (9 - block_instance.metadata)) / 16.0f}};
};

inline auto water_render_rect = [](Block block_instance) -> sf::IntRect
{
    auto data = blockDatabase[block_instance.id];

    return {{0, 0}, {16, 16 - (9 - block_instance.metadata)}};
};

inline auto chest_render_bounds = [](Block block_instance) -> sf::FloatRect
{
    // Open chest (metadata = 1) is drawn shorter so the lid area appears
    // "lifted" — the client has no second texture, so the state change is
    // conveyed by shrinking the top-of-the-sprite.
    if (block_instance.metadata == 1)
    {
        return {{1.0f / 16.0f, 0.0f}, {14.0f / 16.0f, 9.0f / 16.0f}};
    }
    return {{1.0f / 16.0f, 0.0f}, {14.0f / 16.0f, 14.0f / 16.0f}};
};

inline auto chest_render_rect = [](Block block_instance) -> sf::IntRect
{
    if (block_instance.metadata == 1)
    {
        return {{1, 2}, {14, 9}};
    }
    return {{1, 2}, {14, 14}};
};

// Furnace texture is 48x16: frame 0 (x=0) = lit, frames 1-2 (x=16, x=32) = idle
// cycle. The idle animation is driven on the GPU via the vertex-color channel
// (see BlockAnimation above and Render.cpp::appendQuad).
inline auto furnace_render_bounds = [](Block) -> sf::FloatRect
{
    return {{0.0f, 0.0f}, {1.0f, 1.0f}};
};

inline auto furnace_render_rect = [](Block block_instance) -> sf::IntRect
{
    if (block_instance.metadata == 1) return {{0, 0}, {16, 16}};
    return {{16, 0}, {16, 16}};
};

inline auto furnace_animation = [](Block block_instance) -> BlockAnimation
{
    if (block_instance.metadata == 1) return {1, 0};
    return {2, 400};
};

// Woodcutter texture is 32x16: two 16x16 frames side by side. The mesh anchors
// at frame 0 and the shader shifts U each tick.
inline auto woodcutter_render_rect = [](Block) -> sf::IntRect
{
    return {{0, 0}, {16, 16}};
};

inline auto woodcutter_animation = [](Block) -> BlockAnimation
{
    return {2, 400};
};

enum class WaterLevel : uint8_t
{
    FULL = 8,
    SOURCE = 9
};

extern ItemID blockToItem(BlockID block);

extern BlockID itemToBlock(ItemID item);

#endif // BLOCK_HPP