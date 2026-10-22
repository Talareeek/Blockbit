#ifndef BLOCK_UPDATE_HPP
#define BLOCK_UPDATE_HPP

#include <SFML/System/Vector2.hpp>

#include <array>
#include <cstdint>

#include "Block.hpp"

// 2D block update plumbing. See docs/block_updates (if written) or the
// implementation in BlockUpdatePipeline for the propagation model.
//
// Two kinds of neighbor fan-out happen on every real block change:
//   PP (post-placement) — "can I still exist here?" sent to immediate neighbors.
//   NC (neighbor-changed) — "my neighbor changed; recompute my logical state."
// Both run in deterministic, documented direction order. See *_PROPAGATION_ORDER.

enum class Direction : uint8_t
{
    Left  = 0,
    Right = 1,
    Down  = 2,
    Up    = 3
};

inline constexpr int DIRECTION_COUNT = 4;

inline constexpr std::array<sf::Vector2i, DIRECTION_COUNT> direction_offsets = {{
    {-1,  0},  // Left
    { 1,  0},  // Right
    { 0, -1},  // Down  (y decreases downwards in world coords)
    { 0,  1}   // Up
}};

inline constexpr std::array<Direction, DIRECTION_COUNT> opposite_direction = {
    Direction::Right,  // opposite of Left
    Direction::Left,   // opposite of Right
    Direction::Up,     // opposite of Down
    Direction::Down    // opposite of Up
};

// PP order: horizontal neighbors first so support checks (torch-on-wall, falling
// blocks) resolve before vertical ones. Tweak with care — gameplay depends on it.
inline constexpr std::array<Direction, DIRECTION_COUNT> PP_PROPAGATION_ORDER = {
    Direction::Left, Direction::Right, Direction::Down, Direction::Up
};

// NC order kept identical to PP for now. Spec allows them to diverge; wire logic
// that routes signals left-to-right would motivate a different order later.
inline constexpr std::array<Direction, DIRECTION_COUNT> NC_PROPAGATION_ORDER = {
    Direction::Left, Direction::Right, Direction::Down, Direction::Up
};

using DirectionMask = uint8_t;

inline constexpr DirectionMask DIRECTION_MASK_NONE = 0;
inline constexpr DirectionMask DIRECTION_MASK_ALL  = 0x0F;

inline constexpr DirectionMask directionBit(Direction d)
{
    return static_cast<DirectionMask>(1u << static_cast<uint8_t>(d));
}

inline constexpr DirectionMask DIRECTION_MASK_FROM_BELOW = directionBit(Direction::Down);
inline constexpr DirectionMask DIRECTION_MASK_FROM_ABOVE = directionBit(Direction::Up);
inline constexpr DirectionMask DIRECTION_MASK_HORIZONTAL =
    directionBit(Direction::Left) | directionBit(Direction::Right);

enum class BlockUpdateFlags : uint32_t
{
    Default         = 0,
    // Do not run any propagation (world-gen, chunk-load, internal silent writes).
    SuppressAll     = 1u << 0,
    // Run PP + onPlace/onBreak but skip NC. Use for state changes that shouldn't
    // cascade logical signals (chest open/close, door toggle, plant growing).
    SuppressNC      = 1u << 1
};

inline constexpr BlockUpdateFlags operator|(BlockUpdateFlags a, BlockUpdateFlags b)
{
    return static_cast<BlockUpdateFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline constexpr bool hasFlag(BlockUpdateFlags value, BlockUpdateFlags bit)
{
    return (static_cast<uint32_t>(value) & static_cast<uint32_t>(bit)) != 0;
}

// Event emitted by ServerWorld::setBlock, drained by BlockUpdatePipeline. Kept
// POD-ish so vector push is cheap and we can store a lot per tick.
struct PendingPropagation
{
    sf::Vector2i position;
    Block        previous;
    Block        current;
    BlockUpdateFlags flags;
    int          depth;
};

struct ScheduledTick
{
    sf::Vector2i position;
    uint64_t     fire_tick;  // absolute tick number
    uint32_t     priority;   // tie-breaker; lower fires first

    // std::push_heap/std::pop_heap use < by default (= max-heap). We want a
    // min-heap over fire_tick, so operator< returns the OPPOSITE ordering —
    // the "smallest fire_tick" compares as "largest" per std's definition.
    bool operator<(const ScheduledTick& other) const
    {
        if (fire_tick != other.fire_tick) return fire_tick > other.fire_tick;
        if (priority  != other.priority)  return priority  > other.priority;
        if (position.x != other.position.x) return position.x > other.position.x;
        return position.y > other.position.y;
    }
};

#endif // BLOCK_UPDATE_HPP
