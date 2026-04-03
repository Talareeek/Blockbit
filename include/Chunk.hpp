#ifndef CHUNK_HPP
#define CHUNK_HPP

#include "Block.hpp"

#include <SFML/System/Vector2.hpp>

#include <vector>
#include <cstdint>

constexpr int CHUNK_WIDTH = 16;
constexpr int CHUNK_HEIGHT = 256;

struct Chunk
{
    int chunk_position;
    Block blocks[CHUNK_HEIGHT][CHUNK_WIDTH];
    bool dirty;
    bool generated;

    std::vector<uint32_t> entities;

    void addEntity(Entity& entity);
    void removeEntity(Entity& entity, World& world);
};

#endif // CHUNK_HPP