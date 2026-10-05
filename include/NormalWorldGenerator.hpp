#ifndef NORMAL_WORLD_GENERATOR_HPP
#define NORMAL_WORLD_GENERATOR_HPP

#include "WorldGenerator.hpp"
#include "PerlinNoise.hpp"

inline std::unordered_map<Biome, std::pair<Block, Block>> surface_blocks = 
{
    {Biome::Plains, {{BlockID::Grass, 0}, {BlockID::Dirt, 0}}},
    {Biome::Forest, {{BlockID::Grass, 0}, {BlockID::Dirt, 0}}},
    {Biome::Ocean, {{BlockID::Sand, 0}, {BlockID::Sand, 0}}},
    {Biome::Desert, {{BlockID::Sand, 0}, {BlockID::Sand, 0}}},
    {Biome::Savanna, {{BlockID::Coarse_Dirt, 0}, {BlockID::Coarse_Dirt, 0}}},
    {Biome::Mountains, {{BlockID::Stone, 0}, {BlockID::Stone, 0}}},
    {Biome::Snow, {{BlockID::Snow, 0}, {BlockID::Snow, 0}}},
};

struct NormalWorldGeneratorProperties
{
    float base_height;
    float height_scale;
    float frequency;
    float amplitude;
    float persistence;
};

class NormalWorldGenerator : public WorldGenerator
{
private:

    PerlinNoise perlin;
    unsigned int seed;
    NormalWorldGeneratorProperties properties;

public:

    NormalWorldGenerator(unsigned int seed, NormalWorldGeneratorProperties properties);

    Chunk generateChunk(int chunk_position) override;


    Climate getClimate(int x) const;
    Biome getBiome(int x) const;


    void generateTerrain(Chunk& chunk);
    void generateCaves(Chunk& chunk);

    double getHeightNoise(double x) const;
    int getHeight(int worldX) const;


    static constexpr int SEA_LEVEL = 75;
};

#endif // NORMAL_WORLD_GENERATOR_HPP