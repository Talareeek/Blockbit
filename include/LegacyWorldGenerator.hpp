#ifndef LEGACY_WORLD_GENERATOR_HPP
#define LEGACY_WORLD_GENERATOR_HPP

#include "WorldGenerator.hpp"
#include "PerlinNoise.hpp"

class LegacyWorldGenerator : public WorldGenerator
{
private:

    unsigned int seed;

    PerlinNoise perlin;

    float getHeightNoise(float x) const;

    int getHeight(int worldX) const;

    void generateTerrain(Chunk& chunk);

    void generateCaves(Chunk& chunk);

    void generateOres(Chunk& chunk);

    void generateNature(Chunk& chunk);

public:

    LegacyWorldGenerator(unsigned int seed);

    Chunk generateChunk(int chunk_position) override;

    static constexpr int SEA_LEVEL = 75;
};

#endif //LEGACY_WORLD_GENERATOR_HPP
