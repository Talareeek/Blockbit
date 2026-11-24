#ifndef NORMAL_WORLD_GENERATOR_HPP
#define NORMAL_WORLD_GENERATOR_HPP

#include "WorldGenerator.hpp"
#include "PerlinNoise.hpp"
#include "BBT.hpp"
#include "StructureTemplate.hpp"
#include "StructureRegistry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

inline std::unordered_map<Biome, std::pair<Block, Block>> surface_blocks =
{
    {Biome::Plains,     {{Blocks::Grass, 0},       {Blocks::Dirt, 0}}},
    {Biome::Forest,     {{Blocks::Grass, 0},       {Blocks::Dirt, 0}}},
    {Biome::Ocean,      {{Blocks::Sand, 0},        {Blocks::Sand, 0}}},
    {Biome::Desert,     {{Blocks::Sand, 0},        {Blocks::Sand, 0}}},
    {Biome::Savanna,    {{Blocks::Coarse_Dirt, 0}, {Blocks::Coarse_Dirt, 0}}},
    {Biome::Mountains,  {{Blocks::Gravel, 0},      {Blocks::Stone, 0}}},
    {Biome::Snow,       {{Blocks::Snow, 0},        {Blocks::Snow, 0}}},
    {Biome::Jungle,     {{Blocks::Grass, 0},       {Blocks::Dirt, 0}}},
    {Biome::Taiga,      {{Blocks::Grass, 0},       {Blocks::Dirt, 0}}},
    {Biome::SnowyTaiga, {{Blocks::Snow, 0},        {Blocks::Dirt, 0}}},
    {Biome::GiantTaiga, {{Blocks::Coarse_Dirt, 0}, {Blocks::Dirt, 0}}},
};

struct NormalWorldGeneratorProperties
{
    float base_height;
    float height_scale;
    float frequency;
    float amplitude;
    float persistence;

    Tag serialize() const;
    void deserialize(const Tag& tag);
};

enum class FeatureType
{
    Tree,

    CoalOre,
    IronOre,
    GoldOre,
    DiamondOre,
    RubyOre,
    Gravel,
    Diorite,
    Granite,

    Boulder,
    Grass,

    Cactus,
    DeadBush,
    SugarCane,
    CopperOre
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
    uint32_t structureSeed(int grid_x, const StructureKey& id) const;

    void setFeatureBlock(Chunk& chunk, int world_x, int y, Block block, bool only_replace_air = false);

    std::vector<FeaturePlacement> getTreePlacements(int chunk_position) const;
    std::vector<FeaturePlacement> getOrePlacements(int chunk_position, FeatureType type) const;
    std::vector<FeaturePlacement> getCactusPlacements(int chunk_position) const;
    std::vector<FeaturePlacement> getDeadBushPlacements(int chunk_position) const;
    std::vector<FeaturePlacement> getGrassPlacements(int chunk_position) const;
    std::vector<FeaturePlacement> getSugarCanePlacements(int chunk_position) const;

    void generateFeatures(Chunk& chunk);
    void generateFeature(Chunk& chunk, const FeaturePlacement& feature);

    void generateTree(Chunk& chunk, const FeaturePlacement& feature);
    void generateOreVein(Chunk& chunk, const FeaturePlacement& feature, BlockKey ore, int size);
    void generateCactus(Chunk& chunk, const FeaturePlacement& feature);
    void generateDeadBush(Chunk& chunk, const FeaturePlacement& feature);
    void generateGrass(Chunk& chunk, const FeaturePlacement& feature);
    void generateSugarCane(Chunk& chunk, const FeaturePlacement& feature);

    void generateTerrain(Chunk& chunk, std::array<int, CHUNK_WIDTH>& heights);
    void generateCaves(Chunk& chunk, const std::array<int, CHUNK_WIDTH>& heights);

    struct StructurePlacement
    {
        StructureKey      id;
        int               origin_x;
        int               anchor_y;
        StructureRotation rotation;
        uint32_t          seed; // mixed into per-cell rolls at stamp time
    };

    std::vector<StructurePlacement> getStructurePlacementsInCell(int grid_x, const StructureKey& id) const;

    // Walks the jigsaw graph starting from `root` and appends every connected
    // piece (up to the rule's max_chain_depth) to `out`. Deterministic via the
    // root's seed — every neighbour chunk that sees the same root resolves the
    // same chain.
    void expandJigsawChain(const StructurePlacement& root, int max_depth,
                           std::vector<StructurePlacement>& out) const;

    void generateStructures(Chunk& chunk);

public:

    NormalWorldGenerator(unsigned int seed, NormalWorldGeneratorProperties properties);

    Chunk generateChunk(int chunk_position) override;

    Climate getClimate(int x) const;
    Biome getBiome(int x) const;

    double getHeightNoise(double x) const;
    int getHeight(int worldX) const;

    static constexpr int SEA_LEVEL  = 75;
    static constexpr int LAVA_LEVEL = 12;

    static constexpr int TREE_GRID = 7;
    static constexpr int TREE_RADIUS = 3;

    static constexpr int CACTUS_GRID = 9;

    static constexpr int DEAD_BUSH_GRID = 5;
    static constexpr int GRASS_GRID = 4;

    static constexpr int BIOME_BLEND = 40;

    static constexpr int MAX_VEIN_REACH = 10;
};

#endif
