#ifndef FURNACE_BLOCK_ENTITY_HPP
#define FURNACE_BLOCK_ENTITY_HPP

#include "BlockEntity.hpp"
#include "Item.hpp"

inline constexpr uint16_t FURNACE_SLOT_FUEL   = 0;
inline constexpr uint16_t FURNACE_SLOT_INPUT  = 1;
inline constexpr uint16_t FURNACE_SLOT_OUTPUT = 2;
inline constexpr uint16_t FURNACE_SLOT_COUNT  = 3;

class FurnaceBlockEntity : public BlockEntity
{
public:

    ItemStack fuel  {ItemID::None, 0};
    ItemStack input {ItemID::None, 0};
    ItemStack output{ItemID::None, 0};

    float burn_time_remaining = 0.0f;
    float burn_time_total     = 0.0f;
    float cook_time_remaining = 0.0f;
    float cook_time_total     = 0.0f;

    // Ignition latch. Flipped on by the Lighter and off when the furnace runs
    // out of fuel or valid work. While false, the tick leaves the furnace
    // alone — fuel + input alone are not enough to self-ignite.
    bool lit = false;

    uint32_t viewers = 0;

    FurnaceBlockEntity(sf::Vector2i position);

    BlockEntityID typeId() const override { return BlockEntityID::Furnace; }

    void tick(World& world, float dt) override;

    Tag serialize() const override;
    void deserialize(const Tag& tag) override;

    std::unique_ptr<BlockEntity> clone() const override;

    bool isLit() const { return lit; }
};

#endif // FURNACE_BLOCK_ENTITY_HPP
