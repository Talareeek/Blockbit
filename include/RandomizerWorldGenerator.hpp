#ifndef RANDOMIZER_WORLD_GENERATOR_HPP
#define RANDOMIZER_WORLD_GENERATOR_HPP

#include "WorldGenerator.hpp"

#include <cstdint>

class RandomizerWorldGenerator : public WorldGenerator
{
private:

    unsigned int seed;

    uint32_t cellHash(int32_t x, int32_t y) const;

public:

    RandomizerWorldGenerator(unsigned int seed);

    Chunk generateChunk(int chunk_position) override;
};

#endif // RANDOMIZER_WORLD_GENERATOR_HPP
