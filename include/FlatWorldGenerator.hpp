#ifndef FLAT_WORLD_GENERATOR_HPP
#define FLAT_WORLD_GENERATOR_HPP

#include "WorldGenerator.hpp"

class FlatWorldGenerator : public WorldGenerator
{
public:

    Chunk generateChunk(int chunk_position) override;

};

#endif // FLAT_WORLD_GENERATOR_HPP