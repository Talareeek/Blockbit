#ifndef CHUNK_HPP
#define CHUNK_HPP

#include "Block.hpp"

#include <SFML/System/Vector2.hpp>

constexpr int CHUNK_WIDTH = 16;
constexpr int CHUNK_HEIGHT = 256;

struct Chunk
{
    int chunk_position;
    Block blocks[CHUNK_HEIGHT][CHUNK_WIDTH];
    bool dirty;
    bool generated;
};

#endif // CHUNK_HPP