#include "../include/BlockEntity.hpp"
#include "../include/ChestBlockEntity.hpp"

BlockEntity::BlockEntity(sf::Vector2i position) : position{position}
{
}

std::unique_ptr<BlockEntity> BlockEntity::create(BlockEntityID id, sf::Vector2i position)
{
    switch (id)
    {
        case BlockEntityID::Chest: return std::make_unique<ChestBlockEntity>(position);
        case BlockEntityID::None:  return nullptr;
    }
    return nullptr;
}
