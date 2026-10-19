#ifndef CHEST_BLOCK_ENTITY_HPP
#define CHEST_BLOCK_ENTITY_HPP

#include "BlockEntity.hpp"
#include "Item.hpp"

#include <array>

inline constexpr uint16_t CHEST_BLOCK_ENTITY_SLOT_COUNT = 36;

class ChestBlockEntity : public BlockEntity
{
public:

    std::array<ItemStack, CHEST_BLOCK_ENTITY_SLOT_COUNT> stacks{};

    uint32_t viewers = 0;

    ChestBlockEntity(sf::Vector2i position);

    BlockEntityID typeId() const override { return BlockEntityID::Chest; }

    void tick(World& world, float dt) override;

    Tag serialize() const override;
    void deserialize(const Tag& tag) override;

    std::unique_ptr<BlockEntity> clone() const override;

    bool isOpen() const { return viewers > 0; }
};

#endif // CHEST_BLOCK_ENTITY_HPP
