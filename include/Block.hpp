#ifndef BLOCK_HPP
#define BLOCK_HPP

#include <cstdint>
#include <unordered_map>
#include <climits>
#include <optional>
#include <vector>
#include "Item.hpp"
#include "AssetManager.hpp"

// Forward decls so client-only translation units can include Block.hpp without
// pulling in the whole server-side update pipeline. Concrete type lives in
// BlockUpdate.hpp (BlockUpdateContext) which only server code needs.
struct BlockUpdateContext;

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
    Coal_Block,
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
    Furnace,
    Workbench
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

// Function-pointer callbacks — not std::function — to keep BlockData trivially
// copyable and the hot propagation path allocation-free. If a future block needs
// stateful logic, the state belongs on BlockEntity, not here.
using OnPlaceFn            = void (*)(BlockUpdateContext&, sf::Vector2i position);
using OnBreakFn            = void (*)(BlockUpdateContext&, sf::Vector2i position);
using OnPostPlacementFn    = void (*)(BlockUpdateContext&, sf::Vector2i position, uint8_t from_direction);
using OnNeighborChangedFn  = void (*)(BlockUpdateContext&, sf::Vector2i position, sf::Vector2i neighbor_position);
using OnScheduledTickFn    = void (*)(BlockUpdateContext&, sf::Vector2i position);
using OnRandomTickFn       = void (*)(BlockUpdateContext&, sf::Vector2i position);

struct BlockUpdateCallbacks
{
    OnPlaceFn           onPlace            = nullptr;
    OnBreakFn           onBreak            = nullptr;
    OnPostPlacementFn   onPostPlacement    = nullptr;
    OnNeighborChangedFn onNeighborChanged  = nullptr;
    OnScheduledTickFn   onScheduledTick    = nullptr;
    OnRandomTickFn      onRandomTick       = nullptr;
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

    BlockUpdateCallbacks callbacks{};

    // Mask of directions this block accepts PP from. A seedling only grows if
    // the ground below changes, so its mask would be DIRECTION_MASK_FROM_BELOW.
    // Default: accept from all four.
    uint8_t pp_accept_mask = 0x0F;

    // Extra NC targets beyond immediate neighbors — offsets added to the block's
    // own position. Empty for ordinary blocks; wires/signal-carriers populate it.
    std::vector<sf::Vector2i> extra_nc_offsets;

    // Participates in random ticks (grass spread, plant growth, ice melting).
    // Density is global, controlled by BlockUpdatePipeline::random_ticks_per_chunk.
    bool random_tickable = false;
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

// Furnace texture is 48x16: frame 0 (x=0) = idle, frames 1-2 (x=16, x=32) = lit
// cycle. The lit animation is driven on the GPU via the vertex-color channel
// (see BlockAnimation above and Render.cpp::appendQuad).
inline auto furnace_render_bounds = [](Block) -> sf::FloatRect
{
    return {{0.0f, 0.0f}, {1.0f, 1.0f}};
};

inline auto furnace_render_rect = [](Block block_instance) -> sf::IntRect
{
    if (block_instance.metadata == 1) return {{16, 0}, {16, 16}};
    return {{0, 0}, {16, 16}};
};

inline auto furnace_animation = [](Block block_instance) -> BlockAnimation
{
    if (block_instance.metadata == 1) return {2, 400};
    return {1, 0};
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