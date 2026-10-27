#ifndef BLOCK_STATES_HPP
#define BLOCK_STATES_HPP

#include <cstdint>

// Typed views over Block::metadata. Each struct packs into the 16-bit metadata
// slot via pack(); unpack() reconstructs it. Blocks without properties leave
// metadata at 0.

struct WaterState
{
    static constexpr uint8_t FULL   = 8;
    static constexpr uint8_t SOURCE = 9;

    uint8_t level = 0;

    uint16_t pack() const { return static_cast<uint16_t>(level & 0xF); }
    static WaterState unpack(uint16_t v) { return { static_cast<uint8_t>(v & 0xF) }; }
};

// Lava spreads half as far as water: FULL projects level 3 sideways, chain
// decays level 2 → 1 → stop. Max 3 cells of horizontal reach from a source.
struct LavaState
{
    static constexpr uint8_t FULL   = 4;
    static constexpr uint8_t SOURCE = 5;

    uint8_t level = 0;

    uint16_t pack() const { return static_cast<uint16_t>(level & 0xF); }
    static LavaState unpack(uint16_t v) { return { static_cast<uint8_t>(v & 0xF) }; }
};

struct ChestState
{
    bool open = false;

    uint16_t pack() const { return open ? uint16_t{1} : uint16_t{0}; }
    static ChestState unpack(uint16_t v) { return { (v & 0x1) != 0 }; }
};

struct FurnaceState
{
    bool lit = false;

    uint16_t pack() const { return lit ? uint16_t{1} : uint16_t{0}; }
    static FurnaceState unpack(uint16_t v) { return { (v & 0x1) != 0 }; }
};

// Cactus column tracks whether this is the topmost cell. The top texture lives
// at x=16 in cactus.png so the mesh builder can shift the UV without looking at
// the world — the state is baked into the metadata when the cactus grows.
// Default metadata (0) means is_top so a freshly placed cactus renders as the
// top variant without the placement path having to know about the state.
struct CactusState
{
    bool is_top = true;

    uint16_t pack() const { return is_top ? uint16_t{0} : uint16_t{1}; }
    static CactusState unpack(uint16_t v) { return { (v & 0x1) == 0 }; }
};

#endif // BLOCK_STATES_HPP
