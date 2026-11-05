#ifndef SKYGRID_WORLD_GENERATOR_HPP
#define SKYGRID_WORLD_GENERATOR_HPP

#include "WorldGenerator.hpp"

#include <cstdint>

class SkygridWorldGenerator : public WorldGenerator
{
private:

    unsigned int seed;

    uint32_t cellHash(int32_t x, int32_t y) const;

public:

    SkygridWorldGenerator(unsigned int seed);

    Chunk generateChunk(int chunk_position) override;

    static constexpr int GRID_STEP = 2;
};

#endif // SKYGRID_WORLD_GENERATOR_HPP
