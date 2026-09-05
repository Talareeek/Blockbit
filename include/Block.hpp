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
    Iron_Ore,
    Gold_Ore,
    Diamond_Ore,
    Ruby_Ore,
    Oak_Log,
    Oak_Leaves,
    Oak_Planks,
    Woodcutter,
    Fire,
    Sand,
    Coarse_Dirt,
    Snow
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

struct RenderProperties
{
    AssetManager::GameTextureID texture;
    std::function<sf::FloatRect(Block block_instance)> render_bounds = [](Block block_instance) -> sf::FloatRect{return {{0.0f, 0.0f}, {0.0f, 0.0f}};};
    std::function<sf::IntRect(Block block_instance)> rect = [](Block block_instance) -> sf::IntRect{return {{0, 0}, {0, 0}};};
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

enum class WaterLevel : uint8_t
{
    FULL = 8,
    SOURCE = 9
};

extern ItemID blockToItem(BlockID block);

extern BlockID itemToBlock(ItemID item);

#endif // BLOCK_HPP