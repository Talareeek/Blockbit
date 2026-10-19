#ifndef BLOCK_ENTITY_HPP
#define BLOCK_ENTITY_HPP

#include <SFML/System.hpp>

#include <cstdint>
#include <memory>

#include "BBT.hpp"

class World;

enum class BlockEntityID : uint32_t
{
    None = 0,
    Chest = 1
};

class BlockEntity
{
public:

    sf::Vector2i position;

    BlockEntity(sf::Vector2i position);
    virtual ~BlockEntity() = default;

    virtual BlockEntityID typeId() const = 0;

    virtual void tick(World& world, float dt) {}

    virtual Tag serialize() const = 0;
    virtual void deserialize(const Tag& tag) = 0;

    virtual std::unique_ptr<BlockEntity> clone() const = 0;

    static std::unique_ptr<BlockEntity> create(BlockEntityID id, sf::Vector2i position);
};

#endif // BLOCK_ENTITY_HPP
