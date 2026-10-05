#ifndef WORLD_GENERATOR_HPP
#define WORLD_GENERATOR_HPP

#include "Chunk.hpp"

class WorldGenerator
{
public:

    virtual Chunk generateChunk(int chunk_position) = 0;
    virtual ~WorldGenerator() = default;
};

#endif // WORLD_GENERATOR_HPP