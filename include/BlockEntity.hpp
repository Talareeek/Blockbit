#ifndef BLOCK_ENTITY_HPP
#define BLOCK_ENTITY_HPP

#include <SFML/System.hpp>

#include "BBT.hpp"


class BlockEntity
{
public:

    sf::Vector2i position;

    BlockEntity(sf::Vector2i position);

    virtual void tick() {}
    virtual Tag serialize() const = 0;
    virtual void deserialize(const Tag& tag) = 0;
};

#endif // ELOCK_ENTITY_HPP