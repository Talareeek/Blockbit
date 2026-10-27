#include "../include/NormalWorldGenerator.hpp"

namespace
{
    inline uint32_t mix32(uint32_t h)
    {
        h ^= h >> 16;
        h *= 0x85EBCA6Bu;
        h ^= h >> 13;
        h *= 0xC2B2AE35u;
        h ^= h >> 16;
        return h;
    }

    struct SmallRng
    {
        uint32_t state;

        explicit SmallRng(uint32_t s) : state(mix32(s ^ 0x9E3779B9u)) {}

        uint32_t next()
        {
            state += 0x9E3779B9u;
            return mix32(state);
        }

        int range(int n)
        {
            return static_cast<int>(next() % static_cast<uint32_t>(n));
        }

        double unit()
        {
            return static_cast<double>(next()) / 4294967296.0;
        }
    };

    inline int floorDiv(int a, int b)
    {
        int q = a / b;

        if ((a % b != 0) && ((a < 0) != (b < 0)))
            --q;

        return q;
    }

    inline double smoothstep(double e0, double e1, double v)
    {
        const double t = std::clamp((v - e0) / (e1 - e0), 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    }

    constexpr double TEMPERATURE_FREQ     = 0.0015;
    constexpr double HUMIDITY_FREQ        = 0.0020;
    constexpr double CONTINENTALNESS_FREQ = 0.0009;
    constexpr double EROSION_FREQ         = 0.006;
    constexpr double WEIRDNESS_FREQ       = 0.0035;

    constexpr int    CLIMATE_OCTAVES  = 2;

    constexpr double CLIMATE_CONTRAST = 2.0;

    inline double contrast(double v01)
    {
        return std::clamp((v01 - 0.5) * CLIMATE_CONTRAST + 0.5, 0.0, 1.0);
    }

    struct OreDef
    {
        FeatureType type;
        BlockID block;
        int min_y;
        int max_y;
        int veins_per_chunk;
        int size;
    };

    constexpr OreDef ORES[] =
    {
        {FeatureType::CoalOre,    BlockID::Coal_Ore,    5, 110, 10, 10},
        {FeatureType::IronOre,    BlockID::Iron_Ore,    5, 90, 8, 8},
        {FeatureType::GoldOre,    BlockID::Gold_Ore,    5, 60, 4, 6},
        {FeatureType::DiamondOre, BlockID::Diamond_Ore, 5, 25, 2, 5},
        {FeatureType::RubyOre,    BlockID::Ruby_Ore,    5, 15, 1, 4},
        {FeatureType::Gravel,     BlockID::Gravel,      5, 120, 8, 14},
    };

    const OreDef* findOre(FeatureType type)
    {
        for (const OreDef& def : ORES)
        {
            if (def.type == type)
                return &def;
        }

        return nullptr;
    }

    double treeDensity(Biome biome)
    {
        switch (biome)
        {
            case Biome::Jungle:     return 0.98;
            case Biome::GiantTaiga: return 0.90;
            case Biome::Forest:     return 0.90;
            case Biome::Taiga:      return 0.85;
            case Biome::SnowyTaiga: return 0.70;
            case Biome::Plains:     return 0.10;
            case Biome::Savanna:    return 0.06;
            default:                return 0.0;
        }
    }

    struct TreeShape
    {
        int min_height;
        int height_variation;
        int crown_radius;
        int top_radius;
    };

    TreeShape treeShape(Biome biome)
    {
        switch (biome)
        {
            case Biome::Jungle:     return {7, 4, 2, 1};
            case Biome::GiantTaiga: return {9, 4, 2, 1};
            case Biome::Taiga:      return {5, 2, 2, 1};
            case Biome::SnowyTaiga: return {4, 2, 2, 1};
            default:                return {4, 3, 2, 1};
        }
    }
}

NormalWorldGenerator::NormalWorldGenerator(unsigned int seed, NormalWorldGeneratorProperties properties)
    : perlin{seed}, seed{seed}, properties{properties}
{
}

double NormalWorldGenerator::fbm(double x, double y, int octaves) const
{
    double total = 0.0;
    double amplitude = 1.0;
    double frequency = 1.0;
    double norm = 0.0;

    for (int i = 0; i < octaves; i++)
    {
        total += perlin.noise(x * frequency, y + i * 31.7) * amplitude;
        norm += amplitude;

        amplitude *= 0.5;
        frequency *= 2.0;
    }

    return total / norm;
}

Climate NormalWorldGenerator::getClimate(int x) const
{
    Climate climate;

    climate.temperature     = contrast(fbm(x * TEMPERATURE_FREQ,     101.37, CLIMATE_OCTAVES)) * 2.0 - 1.0;
    climate.humidity        = contrast(fbm(x * HUMIDITY_FREQ,        202.71, CLIMATE_OCTAVES)) * 2.0 - 1.0;
    climate.continentalness = contrast(fbm(x * CONTINENTALNESS_FREQ, 303.53, CLIMATE_OCTAVES)) * 2.0 - 1.0;

    climate.erosion         = contrast(fbm(x * EROSION_FREQ,         404.91, CLIMATE_OCTAVES));
    climate.weirdness       = contrast(fbm(x * WEIRDNESS_FREQ,       505.43, CLIMATE_OCTAVES));

    return climate;
}

Biome NormalWorldGenerator::classifyBiome(const Climate& c) const
{
    if (c.continentalness < -0.45)
        return Biome::Ocean;

    if (c.weirdness > 0.65)
        return Biome::Mountains;

    if (c.temperature > 0.35)
    {
        if (c.humidity > 0.4)
            return Biome::Jungle;

        if (c.humidity > -0.1)
            return Biome::Savanna;

        return Biome::Desert;
    }

    if (c.temperature < -0.35)
    {
        if (c.humidity > 0.1)
            return Biome::SnowyTaiga;

        return Biome::Snow;
    }

    if (c.temperature < -0.1)
    {
        if (c.humidity > 0.4)
            return Biome::GiantTaiga;

        if (c.humidity > 0.0)
            return Biome::Taiga;

        return Biome::Plains;
    }

    if (c.humidity > 0.2)
        return Biome::Forest;

    return Biome::Plains;
}

Biome NormalWorldGenerator::getBiome(int x) const
{
    return classifyBiome(getClimate(x));
}

Biome NormalWorldGenerator::getBlendedBiome(int x, uint32_t salt) const
{
    constexpr double BLEND_FREQ = 0.07;

    const double n = perlin.noise(x * BLEND_FREQ, 700.13 + static_cast<double>(salt) * 53.7);

    const int offset = static_cast<int>(std::lround((contrast(n) * 2.0 - 1.0) * BIOME_BLEND));

    return getBiome(x + offset);
}

double NormalWorldGenerator::getHeightNoise(double x) const
{
    double total = 0.0;

    double amplitude = static_cast<double>(properties.amplitude);
    double frequency = static_cast<double>(properties.frequency);

    for (int i = 0; i < 4; i++)
    {
        total += perlin.noise(x * frequency, 0.0) * amplitude;

        amplitude *= static_cast<double>(properties.persistence);
        frequency *= 2.0;
    }

    return total;
}

int NormalWorldGenerator::computeHeight(int x, const Climate& c) const
{
    const double land = smoothstep(-0.55, -0.20, c.continentalness);

    const double mountain = smoothstep(0.45, 0.75, c.weirdness) * land;

    const float base = properties.base_height + static_cast<float>(c.continentalness) * 30.0f;

    const float scale = properties.height_scale * (1.0f - static_cast<float>(c.erosion) * 0.35f);

    const float terrain = static_cast<float>(getHeightNoise(static_cast<double>(x)));

    const double ridge_noise = perlin.noise(x * 0.004, 903.7);

    const double ridge = (1.0 - std::abs(ridge_noise * 2.0 - 1.0)) * mountain * 45.0;

    const int height = static_cast<int>(base + terrain * scale + static_cast<float>(ridge));

    return std::clamp(height, 8, CHUNK_HEIGHT - 32);
}

int NormalWorldGenerator::getHeight(int worldX) const
{
    return computeHeight(worldX, getClimate(worldX));
}

ColumnData NormalWorldGenerator::getColumn(int world_x) const
{
    ColumnData col;

    col.climate = getClimate(world_x);
    col.height = computeHeight(world_x, col.climate);

    col.top_biome = getBlendedBiome(world_x, 1);

    col.top_block = surface_blocks.at(col.top_biome).first;

    for (int i = 0; i < 3; i++)
    {
        const Biome sub_biome = getBlendedBiome(world_x, 2 + i);

        col.sub_blocks[i] = surface_blocks.at(sub_biome).second;
    }

    const bool underwater = col.height <= SEA_LEVEL;
    const bool beach = col.height <= SEA_LEVEL + 2;

    const bool keeps_biome_surface =
        col.top_biome == Biome::Snow ||
        col.top_biome == Biome::SnowyTaiga ||
        col.top_biome == Biome::Mountains;

    if (underwater || (beach && !keeps_biome_surface))
    {
        const BlockID shore_block = (col.climate.temperature < -0.1) ? BlockID::Gravel : BlockID::Sand;
        col.top_block = Block{shore_block, 0};

        for (int i = 0; i < 3; i++)
            col.sub_blocks[i] = Block{shore_block, 0};
    }

    return col;
}

Chunk NormalWorldGenerator::generateChunk(int chunk_position)
{
    Chunk chunk;

    chunk.chunk_position = chunk_position;

    std::array<int, CHUNK_WIDTH> heights{};

    generateTerrain(chunk, heights);

    generateCaves(chunk, heights);

    for (int x = 0; x < CHUNK_WIDTH; x++)
    {
        for (int y = 1; y < LAVA_LEVEL; y++)
        {
            if (chunk.blocks[y][x].id == BlockID::Air)
            {
                chunk.blocks[y][x] = {BlockID::Lava, LavaState{LavaState::SOURCE}.pack()};
            }
        }
    }

    generateFeatures(chunk);

    chunk.generated = true;
    chunk.dirty = true;
    chunk.meshDirty = true;

    return chunk;
}

void NormalWorldGenerator::generateTerrain(Chunk& chunk, std::array<int, CHUNK_WIDTH>& heights)
{
    for (int x = 0; x < CHUNK_WIDTH; x++)
    {
        const int world_x = chunk.chunk_position * CHUNK_WIDTH + x;

        const ColumnData col = getColumn(world_x);

        heights[x] = col.height;

        chunk.climates[x] = col.climate;
        chunk.biomes[x] = col.top_biome;

        for (int y = 0; y < CHUNK_HEIGHT; y++)
        {
            if (y == 0)
            {
                chunk.blocks[y][x] = {BlockID::Bedrock, 0};
            }
            else if (y <= 4)
            {
                // Minecraft-style bedrock crust: bottom row (y=0) is solid, the
                // four rows above scatter bedrock with falling probability
                // (y=1 ~80%, y=2 ~60%, y=3 ~40%, y=4 ~20%). Hash is a function
                // of world_x and y only — identical across every world,
                // independent of the generator seed.
                const uint32_t h = mix32(static_cast<uint32_t>(world_x) * 374761393u
                                       ^ static_cast<uint32_t>(y)       * 668265263u);
                const uint32_t roll = h % 100u;
                const uint32_t threshold = 100u - static_cast<uint32_t>(y) * 20u;

                if (roll < threshold)
                {
                    chunk.blocks[y][x] = {BlockID::Bedrock, 0};
                }
                else if (y < col.height - 4)
                {
                    chunk.blocks[y][x] = {BlockID::Stone, 0};
                }
                else if (y < col.height - 1)
                {
                    chunk.blocks[y][x] = col.sub_blocks[col.height - 2 - y];
                }
                else if (y == col.height - 1)
                {
                    chunk.blocks[y][x] = col.top_block;
                }
                else
                {
                    chunk.blocks[y][x] = {BlockID::Air, 0};
                }
            }
            else if (y < col.height - 4)
            {
                chunk.blocks[y][x] = {BlockID::Stone, 0};
            }
            else if (y < col.height - 1)
            {
                chunk.blocks[y][x] = col.sub_blocks[col.height - 2 - y];
            }
            else if (y == col.height - 1)
            {
                chunk.blocks[y][x] = col.top_block;
            }
            else if (y < SEA_LEVEL)
            {
                chunk.blocks[y][x] = {BlockID::Water, WaterState{WaterState::SOURCE}.pack()};
            }
            else
            {
                chunk.blocks[y][x] = {BlockID::Air, 0};
            }
        }
    }
}

void NormalWorldGenerator::generateCaves(Chunk& chunk, const std::array<int, CHUNK_WIDTH>& heights)
{
    for (int x = 0; x < CHUNK_WIDTH; x++)
    {
        const int world_x = chunk.chunk_position * CHUNK_WIDTH + x;

        constexpr int CRUST = 6;
        constexpr double FADE_DEPTH = 16.0;

        const int surface = heights[x];
        const int max_y = std::min(surface - CRUST, CHUNK_HEIGHT - 1);

        for (int y = 1; y <= max_y; y++)
        {
            // Bedrock crust (y=0..4) is honoured: cave carving stays non-
            // destructive against the scatter pattern laid down by terrain gen.
            if (chunk.blocks[y][x].id == BlockID::Bedrock) continue;

            const double noise = perlin.noise(world_x * 0.05, y * 0.05);

            const double depth = static_cast<double>(surface - y - CRUST);
            const double shallow = 1.0 - std::clamp(depth / FADE_DEPTH, 0.0, 1.0);
            const double threshold = 0.65 + 0.25 * shallow;

            if (noise > threshold)
            {
                chunk.blocks[y][x] = {BlockID::Air, 0};
            }
        }
    }
}

uint32_t NormalWorldGenerator::featureSeed(int x, int y, FeatureType type) const
{
    uint32_t h = mix32(static_cast<uint32_t>(seed) + 0x9E3779B9u);

    h = mix32(h ^ (static_cast<uint32_t>(x) * 374761393u));
    h = mix32(h ^ (static_cast<uint32_t>(y) * 668265263u));
    h = mix32(h ^ (static_cast<uint32_t>(type) * 1274126177u + 0x27D4EB2Fu));

    return h;
}

void NormalWorldGenerator::setFeatureBlock(Chunk& chunk, int world_x, int y, Block block, bool only_replace_air)
{
    const int local_x = world_x - chunk.chunk_position * CHUNK_WIDTH;

    if (local_x < 0 || local_x >= CHUNK_WIDTH)
        return;

    if (y < 0 || y >= CHUNK_HEIGHT)
        return;

    if (only_replace_air && chunk.blocks[y][local_x].id != BlockID::Air)
        return;

    chunk.blocks[y][local_x] = block;
}

void NormalWorldGenerator::generateFeatures(Chunk& chunk)
{
    for (const FeaturePlacement& tree : getTreePlacements(chunk.chunk_position))
    {
        generateFeature(chunk, tree);
    }

    for (const FeaturePlacement& cactus : getCactusPlacements(chunk.chunk_position))
    {
        generateFeature(chunk, cactus);
    }

    const int neighbor_range = (MAX_VEIN_REACH + CHUNK_WIDTH - 1) / CHUNK_WIDTH;

    for (const OreDef& ore : ORES)
    {
        for (int cx = chunk.chunk_position - neighbor_range; cx <= chunk.chunk_position + neighbor_range; cx++)
        {
            for (const FeaturePlacement& vein : getOrePlacements(cx, ore.type))
            {
                generateFeature(chunk, vein);
            }
        }
    }
}

void NormalWorldGenerator::generateFeature(Chunk& chunk, const FeaturePlacement& feature)
{
    if (feature.type == FeatureType::OakTree)
    {
        generateOakTree(chunk, feature);
        return;
    }

    if (feature.type == FeatureType::Cactus)
    {
        generateCactus(chunk, feature);
        return;
    }

    if (const OreDef* ore = findOre(feature.type))
    {
        generateOreVein(chunk, feature, ore->block, ore->size);
    }
}

std::vector<FeaturePlacement> NormalWorldGenerator::getTreePlacements(int chunk_position) const
{
    std::vector<FeaturePlacement> result;

    const int chunk_min = chunk_position * CHUNK_WIDTH;
    const int chunk_max = chunk_min + CHUNK_WIDTH - 1;

    const int search_min = chunk_min - TREE_RADIUS;
    const int search_max = chunk_max + TREE_RADIUS;

    const int grid_min = floorDiv(search_min, TREE_GRID);
    const int grid_max = floorDiv(search_max, TREE_GRID);

    for (int grid_x = grid_min; grid_x <= grid_max; grid_x++)
    {
        SmallRng rng(featureSeed(grid_x, 0, FeatureType::OakTree));

        const int world_x = grid_x * TREE_GRID + 1 + rng.range(TREE_GRID - 2);

        const double roll = rng.unit();
        const uint32_t tree_seed = rng.next();

        if (world_x < search_min || world_x > search_max)
            continue;

        const ColumnData col = getColumn(world_x);

        const BlockID top = col.top_block.id;

        const bool snowy_tree_biome = col.top_biome == Biome::SnowyTaiga;

        if (top != BlockID::Grass && top != BlockID::Coarse_Dirt && !(snowy_tree_biome && top == BlockID::Snow))
            continue;

        if (col.height <= SEA_LEVEL + 2)
            continue;

        double density = treeDensity(col.top_biome);

        const double humidity01 = std::clamp((col.climate.humidity + 1.0) * 0.5, 0.0, 1.0);

        density *= 0.6 + 0.4 * humidity01;

        if (roll > density)
            continue;

        result.push_back(FeaturePlacement{FeatureType::OakTree, world_x, col.height, tree_seed});
    }

    return result;
}

void NormalWorldGenerator::generateOakTree(Chunk& chunk, const FeaturePlacement& feature)
{
    SmallRng rng(feature.seed);

    const Biome biome = getBlendedBiome(feature.x, 1);
    const TreeShape shape = treeShape(biome);

    const int height = shape.min_height + rng.range(shape.height_variation);

    const int crown_y = feature.y + height - 2;

    for (int dy = -1; dy <= 2; dy++)
    {
        const int radius = (dy <= 0) ? shape.crown_radius : (dy == 1 ? shape.top_radius : 0);

        for (int dx = -radius; dx <= radius; dx++)
        {
            if (std::abs(dx) == shape.crown_radius && shape.crown_radius >= 2 && rng.range(4) == 0)
                continue;

            setFeatureBlock(chunk, feature.x + dx, crown_y + dy, {BlockID::Oak_Leaves, 0}, true);
        }
    }

    for (int i = 0; i < height; i++)
    {
        setFeatureBlock(chunk, feature.x, feature.y + i, {BlockID::Oak_Log, 0});
    }
}

std::vector<FeaturePlacement> NormalWorldGenerator::getOrePlacements(int chunk_position, FeatureType type) const
{
    std::vector<FeaturePlacement> result;

    const OreDef* def = findOre(type);

    if (!def || def->max_y <= def->min_y)
        return result;

    for (int i = 0; i < def->veins_per_chunk; i++)
    {
        SmallRng rng(featureSeed(chunk_position, i, type));

        const int world_x = chunk_position * CHUNK_WIDTH + rng.range(CHUNK_WIDTH);

        const int y = def->min_y + rng.range(def->max_y - def->min_y + 1);

        result.push_back(FeaturePlacement{type, world_x, y, rng.next()});
    }

    return result;
}

std::vector<FeaturePlacement> NormalWorldGenerator::getCactusPlacements(int chunk_position) const
{
    std::vector<FeaturePlacement> result;

    const int chunk_min = chunk_position * CHUNK_WIDTH;
    const int chunk_max = chunk_min + CHUNK_WIDTH - 1;

    const int grid_min = floorDiv(chunk_min, CACTUS_GRID);
    const int grid_max = floorDiv(chunk_max, CACTUS_GRID);

    for (int grid_x = grid_min; grid_x <= grid_max; grid_x++)
    {
        SmallRng rng(featureSeed(grid_x, 0, FeatureType::Cactus));

        const int world_x = grid_x * CACTUS_GRID + rng.range(CACTUS_GRID);

        const double roll = rng.unit();
        const uint32_t cactus_seed = rng.next();

        if (world_x < chunk_min || world_x > chunk_max)
            continue;

        const ColumnData col = getColumn(world_x);

        if (col.top_biome != Biome::Desert)
            continue;

        if (col.top_block.id != BlockID::Sand)
            continue;

        if (col.height <= SEA_LEVEL + 2)
            continue;

        // ~40% of candidate grid cells spawn a cactus; combined with grid=9 that
        // leaves a desert dotted but not covered.
        if (roll > 0.4)
            continue;

        result.push_back(FeaturePlacement{FeatureType::Cactus, world_x, col.height, cactus_seed});
    }

    return result;
}

void NormalWorldGenerator::generateCactus(Chunk& chunk, const FeaturePlacement& feature)
{
    SmallRng rng(feature.seed);

    const int local_x = feature.x - chunk.chunk_position * CHUNK_WIDTH;
    if (local_x < 0 || local_x >= CHUNK_WIDTH) return;

    // 1..3 inclusive — new world chunks ship with cacti at various growth stages
    // instead of everything starting at height 1.
    const int height = 1 + rng.range(3);

    for (int i = 0; i < height; i++)
    {
        const int y = feature.y + i;
        if (y < 0 || y >= CHUNK_HEIGHT) return;
        if (chunk.blocks[y][local_x].id != BlockID::Air) return;

        const bool is_top = (i == height - 1);
        chunk.blocks[y][local_x] = {BlockID::Cactus, CactusState{is_top}.pack()};
    }
}

void NormalWorldGenerator::generateOreVein(Chunk& chunk, const FeaturePlacement& feature, BlockID ore, int size)
{
    SmallRng rng(feature.seed);

    const int chunk_min = chunk.chunk_position * CHUNK_WIDTH;

    int x = feature.x;
    int y = feature.y;

    for (int i = 0; i < size; i++)
    {
        const int local_x = x - chunk_min;

        if (local_x >= 0 && local_x < CHUNK_WIDTH && y >= 1 && y < CHUNK_HEIGHT)
        {
            if (chunk.blocks[y][local_x].id == BlockID::Stone)
            {
                chunk.blocks[y][local_x] = {ore, 0};
            }
        }

        int dx;
        int dy;

        do
        {
            dx = rng.range(3) - 1;
            dy = rng.range(3) - 1;
        }
        while (dx == 0 && dy == 0);

        x += dx;
        y += dy;
    }
}