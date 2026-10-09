#ifndef NORMAL_WORLD_GENERATOR_HPP
#define NORMAL_WORLD_GENERATOR_HPP

#include "WorldGenerator.hpp"
#include "PerlinNoise.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

inline std::unordered_map<Biome, std::pair<Block, Block>> surface_blocks =
{
    {Biome::Plains,    {{BlockID::Grass, 0},       {BlockID::Dirt, 0}}},
    {Biome::Forest,    {{BlockID::Grass, 0},       {BlockID::Dirt, 0}}},
    {Biome::Ocean,     {{BlockID::Sand, 0},        {BlockID::Sand, 0}}},
    {Biome::Desert,    {{BlockID::Sand, 0},        {BlockID::Sand, 0}}},
    {Biome::Savanna,   {{BlockID::Coarse_Dirt, 0}, {BlockID::Coarse_Dirt, 0}}},
    {Biome::Mountains, {{BlockID::Stone, 0},       {BlockID::Stone, 0}}},
    {Biome::Snow,      {{BlockID::Snow, 0},        {BlockID::Snow, 0}}},
};

struct NormalWorldGeneratorProperties
{
    float base_height;
    float height_scale;
    float frequency;
    float amplitude;
    float persistence;
};

enum class FeatureType
{
    OakTree,

    IronOre,
    GoldOre,
    DiamondOre,
    RubyOre,

    Boulder,
    Grass
};

struct FeaturePlacement
{
    FeatureType type;

    int x;
    int y;

    unsigned int seed;
};

struct ColumnData
{
    Climate climate;

    int height;

    Biome top_biome;

    Block top_block;
    Block sub_blocks[3];
};

class NormalWorldGenerator : public WorldGenerator
{
private:

    PerlinNoise perlin;
    unsigned int seed;
    NormalWorldGeneratorProperties properties;

    double fbm(double x, double y, int octaves) const;
    Biome classifyBiome(const Climate& climate) const;
    Biome getBlendedBiome(int x, uint32_t salt) const;
    int computeHeight(int x, const Climate& climate) const;
    ColumnData getColumn(int world_x) const;

    uint32_t featureSeed(int x, int y, FeatureType type) const;

    void setFeatureBlock(Chunk& chunk, int world_x, int y, Block block, bool only_replace_air = false);

    std::vector<FeaturePlacement> getTreePlacements(int chunk_position) const;
    std::vector<FeaturePlacement> getOrePlacements(int chunk_position, FeatureType type) const;

    void generateFeatures(Chunk& chunk);
    void generateFeature(Chunk& chunk, const FeaturePlacement& feature);

    void generateOakTree(Chunk& chunk, const FeaturePlacement& feature);
    void generateOreVein(Chunk& chunk, const FeaturePlacement& feature, BlockID ore, int size);

    void generateTerrain(Chunk& chunk, std::array<int, CHUNK_WIDTH>& heights);
    void generateCaves(Chunk& chunk, const std::array<int, CHUNK_WIDTH>& heights);

public:

    NormalWorldGenerator(unsigned int seed, NormalWorldGeneratorProperties properties);

    Chunk generateChunk(int chunk_position) override;

    Climate getClimate(int x) const;
    Biome getBiome(int x) const;

    double getHeightNoise(double x) const;
    int getHeight(int worldX) const;

    static constexpr int SEA_LEVEL = 75;

    static constexpr int TREE_GRID = 7;
    static constexpr int TREE_RADIUS = 3;

    static constexpr int BIOME_BLEND = 24;

    static constexpr int MAX_VEIN_REACH = 10;
};

#endif