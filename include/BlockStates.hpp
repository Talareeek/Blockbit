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

#endif // BLOCK_STATES_HPP
