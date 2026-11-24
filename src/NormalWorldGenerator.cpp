#include "../include/NormalWorldGenerator.hpp"

Tag NormalWorldGeneratorProperties::serialize() const
{
    TagCompound compound;
    compound["base_height"]  = Tag(base_height);
    compound["height_scale"] = Tag(height_scale);
    compound["frequency"]    = Tag(frequency);
    compound["amplitude"]    = Tag(amplitude);
    compound["persistence"]  = Tag(persistence);
    return Tag(compound);
}

void NormalWorldGeneratorProperties::deserialize(const Tag& tag)
{
    const TagCompound& compound = tag.get<TagCompound>();
    base_height  = compound.at("base_height").get<float>();
    height_scale = compound.at("height_scale").get<float>();
    frequency    = compound.at("frequency").get<float>();
    amplitude    = compound.at("amplitude").get<float>();
    persistence  = compound.at("persistence").get<float>();
}

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
        BlockKey block;
        int min_y;
        int max_y;
        int veins_per_chunk;
        int size;
    };

    const OreDef ORES[] =
    {
        {FeatureType::CoalOre,    Blocks::Coal_Ore,    5, 110, 10, 10},
        {FeatureType::IronOre,    Blocks::Iron_Ore,    5, 90, 8, 8},
        {FeatureType::GoldOre,    Blocks::Gold_Ore,    5, 60, 4, 6},
        {FeatureType::DiamondOre, Blocks::Diamond_Ore, 5, 25, 2, 5},
        {FeatureType::RubyOre,    Blocks::Ruby_Ore,    5, 15, 1, 4},
        {FeatureType::CopperOre,  Blocks::Copper_Ore,  5, 100, 8, 8},
        {FeatureType::Gravel,     Blocks::Gravel,      5, 120, 8, 14},
        {FeatureType::Diorite,    Blocks::Diorite,     5, 110, 7, 20},
        {FeatureType::Granite,    Blocks::Granite,     5, 110, 7, 20},
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

    struct TreeSpecies
    {
        BlockKey log;
        BlockKey leaves;
    };

    // Species picker consumes one rng draw for the Forest oak/birch roll so
    // every candidate column takes the same number of rolls regardless of
    // biome — this keeps the per-tree seed deterministic across refactors.
    TreeSpecies treeSpecies(Biome biome, SmallRng& rng)
    {
        const double roll = rng.unit();

        switch (biome)
        {
            case Biome::Taiga:
            case Biome::SnowyTaiga:
            case Biome::GiantTaiga:
                return {Blocks::Spruce_Log, Blocks::Spruce_Leaves};

            case Biome::Forest:
                if (roll < 0.3)
                    return {Blocks::Birch_Log, Blocks::Birch_Leaves};
                return {Blocks::Oak_Log, Blocks::Oak_Leaves};

            default:
                return {Blocks::Oak_Log, Blocks::Oak_Leaves};
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
        const BlockKey shore_block = (col.climate.temperature < -0.1) ? Blocks::Gravel : Blocks::Sand;
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
            if (chunk.blocks[y][x].id == Blocks::Air)
            {
                chunk.blocks[y][x] = {Blocks::Lava, LavaState{LavaState::SOURCE}.pack()};
            }
        }
    }

    generateFeatures(chunk);

    // Structures stamp AFTER features so a wall authored with Overwrite mode
    // always wins over trees/ores that happened to roll inside its bbox.
    // AirOnly cells still respect features (intentional — authors can carve
    // around trees by using AirOnly for decorative overhangs).
    generateStructures(chunk);

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
                chunk.blocks[y][x] = {Blocks::Bedrock, 0};
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
                    chunk.blocks[y][x] = {Blocks::Bedrock, 0};
                }
                else if (y < col.height - 4)
                {
                    chunk.blocks[y][x] = {Blocks::Stone, 0};
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
                    chunk.blocks[y][x] = {Blocks::Air, 0};
                }
            }
            else if (y < col.height - 4)
            {
                chunk.blocks[y][x] = {Blocks::Stone, 0};
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
                chunk.blocks[y][x] = {Blocks::Water, WaterState{WaterState::SOURCE}.pack()};
            }
            else
            {
                chunk.blocks[y][x] = {Blocks::Air, 0};
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
            if (chunk.blocks[y][x].id == Blocks::Bedrock) continue;

            const double noise = perlin.noise(world_x * 0.05, y * 0.05);

            const double depth = static_cast<double>(surface - y - CRUST);
            const double shallow = 1.0 - std::clamp(depth / FADE_DEPTH, 0.0, 1.0);
            const double threshold = 0.65 + 0.25 * shallow;

            if (noise > threshold)
            {
                chunk.blocks[y][x] = {Blocks::Air, 0};
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

    if (only_replace_air && chunk.blocks[y][local_x].id != Blocks::Air)
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

    for (const FeaturePlacement& dead_bush : getDeadBushPlacements(chunk.chunk_position))
    {
        generateFeature(chunk, dead_bush);
    }

    for (const FeaturePlacement& grass : getGrassPlacements(chunk.chunk_position))
    {
        generateFeature(chunk, grass);
    }

    for (const FeaturePlacement& sugar_cane : getSugarCanePlacements(chunk.chunk_position))
    {
        generateFeature(chunk, sugar_cane);
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
    if (feature.type == FeatureType::Tree)
    {
        generateTree(chunk, feature);
        return;
    }

    if (feature.type == FeatureType::Cactus)
    {
        generateCactus(chunk, feature);
        return;
    }

    if (feature.type == FeatureType::DeadBush)
    {
        generateDeadBush(chunk, feature);
        return;
    }

    if (feature.type == FeatureType::Grass)
    {
        generateGrass(chunk, feature);
        return;
    }

    if (feature.type == FeatureType::SugarCane)
    {
        generateSugarCane(chunk, feature);
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
        SmallRng rng(featureSeed(grid_x, 0, FeatureType::Tree));

        const int world_x = grid_x * TREE_GRID + 1 + rng.range(TREE_GRID - 2);

        const double roll = rng.unit();
        const uint32_t tree_seed = rng.next();

        if (world_x < search_min || world_x > search_max)
            continue;

        const ColumnData col = getColumn(world_x);

        const BlockKey top = col.top_block.id;

        const bool snowy_tree_biome = col.top_biome == Biome::SnowyTaiga;

        if (top != Blocks::Grass && top != Blocks::Coarse_Dirt && !(snowy_tree_biome && top == Blocks::Snow))
            continue;

        if (col.height <= SEA_LEVEL + 2)
            continue;

        double density = treeDensity(col.top_biome);

        const double humidity01 = std::clamp((col.climate.humidity + 1.0) * 0.5, 0.0, 1.0);

        density *= 0.6 + 0.4 * humidity01;

        if (roll > density)
            continue;

        result.push_back(FeaturePlacement{FeatureType::Tree, world_x, col.height, tree_seed});
    }

    return result;
}

void NormalWorldGenerator::generateTree(Chunk& chunk, const FeaturePlacement& feature)
{
    SmallRng rng(feature.seed);

    const Biome biome = getBlendedBiome(feature.x, 1);
    const TreeShape shape = treeShape(biome);
    const TreeSpecies species = treeSpecies(biome, rng);

    const int height = shape.min_height + rng.range(shape.height_variation);

    const int crown_y = feature.y + height - 2;

    for (int dy = -1; dy <= 2; dy++)
    {
        const int radius = (dy <= 0) ? shape.crown_radius : (dy == 1 ? shape.top_radius : 0);

        for (int dx = -radius; dx <= radius; dx++)
        {
            if (std::abs(dx) == shape.crown_radius && shape.crown_radius >= 2 && rng.range(4) == 0)
                continue;

            setFeatureBlock(chunk, feature.x + dx, crown_y + dy, {species.leaves, 0}, true);
        }
    }

    for (int i = 0; i < height; i++)
    {
        setFeatureBlock(chunk, feature.x, feature.y + i, {species.log, 0});
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

        if (col.top_block.id != Blocks::Sand)
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
        if (chunk.blocks[y][local_x].id != Blocks::Air) return;

        const bool is_top = (i == height - 1);
        chunk.blocks[y][local_x] = {Blocks::Cactus, CactusState{is_top}.pack()};
    }
}

std::vector<FeaturePlacement> NormalWorldGenerator::getDeadBushPlacements(int chunk_position) const
{
    std::vector<FeaturePlacement> result;

    const int chunk_min = chunk_position * CHUNK_WIDTH;
    const int chunk_max = chunk_min + CHUNK_WIDTH - 1;

    const int grid_min = floorDiv(chunk_min, DEAD_BUSH_GRID);
    const int grid_max = floorDiv(chunk_max, DEAD_BUSH_GRID);

    for (int grid_x = grid_min; grid_x <= grid_max; grid_x++)
    {
        SmallRng rng(featureSeed(grid_x, 0, FeatureType::DeadBush));

        const int world_x = grid_x * DEAD_BUSH_GRID + rng.range(DEAD_BUSH_GRID);

        const double roll = rng.unit();
        const uint32_t bush_seed = rng.next();

        if (world_x < chunk_min || world_x > chunk_max)
            continue;

        const ColumnData col = getColumn(world_x);

        if (col.top_biome != Biome::Desert)
            continue;

        if (col.top_block.id != Blocks::Sand)
            continue;

        if (col.height <= SEA_LEVEL + 2)
            continue;

        // Sparser than cactus (grid=5, 25% roll) so the desert reads as "mostly
        // empty with the occasional dried-up shrub."
        if (roll > 0.25)
            continue;

        result.push_back(FeaturePlacement{FeatureType::DeadBush, world_x, col.height, bush_seed});
    }

    return result;
}

void NormalWorldGenerator::generateDeadBush(Chunk& chunk, const FeaturePlacement& feature)
{
    const int local_x = feature.x - chunk.chunk_position * CHUNK_WIDTH;
    if (local_x < 0 || local_x >= CHUNK_WIDTH) return;

    const int y = feature.y;
    if (y < 0 || y >= CHUNK_HEIGHT) return;

    // Don't trample a cactus or other feature that landed on the same column.
    if (chunk.blocks[y][local_x].id != Blocks::Air) return;

    chunk.blocks[y][local_x] = {Blocks::Dead_Bush, 0};
}

std::vector<FeaturePlacement> NormalWorldGenerator::getGrassPlacements(int chunk_position) const
{
    std::vector<FeaturePlacement> result;
    const int chunk_min = chunk_position * CHUNK_WIDTH;
    const int chunk_max = chunk_min + CHUNK_WIDTH - 1;
    const int grid_min = floorDiv(chunk_min, GRASS_GRID);
    const int grid_max = floorDiv(chunk_max, GRASS_GRID);

    for (int grid_x = grid_min; grid_x <= grid_max; ++grid_x)
    {
        SmallRng rng(featureSeed(grid_x, 0, FeatureType::Grass));
        const int world_x = grid_x * GRASS_GRID + rng.range(GRASS_GRID);
        const double roll = rng.unit();
        const uint32_t grass_seed = rng.next();
        if (world_x < chunk_min || world_x > chunk_max || roll > 0.35) continue;

        const ColumnData col = getColumn(world_x);
        if (col.height <= SEA_LEVEL + 1 || col.height >= CHUNK_HEIGHT - 1) continue;
        if (col.top_biome != Biome::Plains && col.top_biome != Biome::Forest &&
            col.top_biome != Biome::Jungle && col.top_biome != Biome::Taiga &&
            col.top_biome != Biome::Savanna) continue;
        if (col.top_block.id != Blocks::Grass && col.top_block.id != Blocks::Coarse_Dirt) continue;

        result.push_back(FeaturePlacement{FeatureType::Grass, world_x, col.height, grass_seed});
    }
    return result;
}

void NormalWorldGenerator::generateGrass(Chunk& chunk, const FeaturePlacement& feature)
{
    const int local_x = feature.x - chunk.chunk_position * CHUNK_WIDTH;
    if (local_x < 0 || local_x >= CHUNK_WIDTH || feature.y < 0 || feature.y >= CHUNK_HEIGHT) return;
    if (!blockDatabase.contains(Blocks::Tall_Grass) || chunk.blocks[feature.y][local_x].id != Blocks::Air) return;
    chunk.blocks[feature.y][local_x] = {Blocks::Tall_Grass, 0};
}

std::vector<FeaturePlacement> NormalWorldGenerator::getSugarCanePlacements(int chunk_position) const
{
    std::vector<FeaturePlacement> result;
    const int chunk_min = chunk_position * CHUNK_WIDTH;
    const int chunk_max = chunk_min + CHUNK_WIDTH - 1;
    const BlockKey sugar_cane = "blockbit:sugar_cane";
    if (!blockDatabase.contains(sugar_cane)) return result;

    for (int world_x = chunk_min; world_x <= chunk_max; ++world_x)
    {
        const ColumnData col = getColumn(world_x);
        if (col.height <= SEA_LEVEL || col.height >= CHUNK_HEIGHT - 1) continue;
        if (col.top_block.id != Blocks::Sand && col.top_block.id != Blocks::Dirt &&
            col.top_block.id != Blocks::Grass && col.top_block.id != Blocks::Coarse_Dirt) continue;

        const ColumnData left = getColumn(world_x - 1);
        const ColumnData right = getColumn(world_x + 1);
        if (left.height > SEA_LEVEL && right.height > SEA_LEVEL) continue;

        SmallRng rng(featureSeed(world_x, 0, FeatureType::SugarCane));
        const double roll = rng.unit();
        const uint32_t cane_seed = rng.next();
        if (roll < 0.5)
            result.push_back(FeaturePlacement{FeatureType::SugarCane, world_x, col.height, cane_seed});
    }
    return result;
}

void NormalWorldGenerator::generateSugarCane(Chunk& chunk, const FeaturePlacement& feature)
{
    const int local_x = feature.x - chunk.chunk_position * CHUNK_WIDTH;
    if (local_x < 0 || local_x >= CHUNK_WIDTH || feature.y < 0 || feature.y >= CHUNK_HEIGHT) return;
    const BlockKey sugar_cane = "blockbit:sugar_cane";
    if (!blockDatabase.contains(sugar_cane) || chunk.blocks[feature.y][local_x].id != Blocks::Air) return;

    SmallRng rng(feature.seed);
    const int height = 1 + rng.range(3);
    for (int i = 0; i < height && feature.y + i < CHUNK_HEIGHT; ++i)
    {
        if (chunk.blocks[feature.y + i][local_x].id != Blocks::Air) break;
        chunk.blocks[feature.y + i][local_x] = {sugar_cane, 0};
    }
}

void NormalWorldGenerator::generateOreVein(Chunk& chunk, const FeaturePlacement& feature, BlockKey ore, int size)
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
            if (chunk.blocks[y][local_x].id == Blocks::Stone)
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

uint32_t NormalWorldGenerator::structureSeed(int grid_x, const StructureKey& id) const
{
    uint32_t h = mix32(static_cast<uint32_t>(seed) + 0x165667B1u);

    h = mix32(h ^ (static_cast<uint32_t>(grid_x) * 374761393u));
    h = mix32(h ^ (([&] { uint32_t key_hash = 2166136261u; for (unsigned char c : id) { key_hash ^= c; key_hash *= 16777619u; } return key_hash; }()) * 2246822519u + 0x9E3779B9u));

    return h;
}

std::vector<NormalWorldGenerator::StructurePlacement>
NormalWorldGenerator::getStructurePlacementsInCell(int grid_x, const StructureKey& id) const
{
    std::vector<StructurePlacement> result;

    const StructureRule& rule = StructureRegistry::getRule(id);

    if (rule.grid <= 0 || rule.spawn_chance <= 0.0f)
        return result;

    SmallRng rng(structureSeed(grid_x, id));

    // Pull the fixed set of draws BEFORE any filter so the per-cell seed stays
    // stable regardless of which filter rejects. Additional draws exist for
    // the new MirrorY variant roll and the chain expansion seed.
    const int      cell_offset    = rng.range(rule.grid);
    const double   roll           = rng.unit();
    const double   mirror_roll    = rng.unit();
    const double   mirror_y_roll  = rng.unit();
    const double   depth_roll     = rng.unit();
    const uint32_t chain_seed     = rng.next();

    const int origin_x = grid_x * rule.grid + cell_offset;

    if (static_cast<float>(roll) > rule.spawn_chance)
        return result;

    const ColumnData col = getColumn(origin_x);

    if (!rule.biomes.empty())
    {
        bool allowed = false;
        for (Biome b : rule.biomes)
        {
            if (b == col.top_biome) { allowed = true; break; }
        }
        if (!allowed) return result;
    }

    if (col.height < rule.min_surface_y || col.height > rule.max_surface_y)
        return result;

    const StructureTemplate& root_tmpl = StructureRegistry::getTemplate(id);

    int anchor_y = 0;
    switch (rule.anchor_mode)
    {
        case StructureRule::Anchor::Surface:
            anchor_y = col.height;
            break;
        case StructureRule::Anchor::Fixed:
            anchor_y = rule.target_y;
            break;
        case StructureRule::Anchor::BelowSurface:
        {
            const int min_anchor = root_tmpl.anchor_y + 1;
            const int max_anchor = col.height - 1 - (root_tmpl.height - 1 - root_tmpl.anchor_y);
            if (max_anchor < min_anchor) return result;

            const int span = max_anchor - min_anchor + 1;
            anchor_y = min_anchor + static_cast<int>(depth_roll * span);
            break;
        }
    }

    StructureRotation rotation = StructureRotation::Original;
    const bool flip_x = rule.allow_mirror        && mirror_roll   < 0.5;
    const bool flip_y = root_tmpl.allow_mirror_y && mirror_y_roll < 0.5;
    if (flip_x && flip_y) rotation = StructureRotation::MirrorXY;
    else if (flip_x)      rotation = StructureRotation::MirrorX;
    else if (flip_y)      rotation = StructureRotation::MirrorY;

    StructurePlacement root{id, origin_x, anchor_y, rotation, chain_seed};
    result.push_back(root);

    if (rule.max_chain_depth > 0)
        expandJigsawChain(root, rule.max_chain_depth, result);

    return result;
}

void NormalWorldGenerator::expandJigsawChain(const StructurePlacement& root, int max_depth,
                                             std::vector<StructurePlacement>& out) const
{
    // Frontier walk — each entry is a piece whose jigsaws have not been
    // expanded yet. `remaining` caps how deep this branch can still go.
    // `skip_jigsaw` is the index of the jigsaw this piece used to connect to
    // its parent; re-expanding it would attach a new piece on top of the
    // parent and cause the chain to double back. Root starts with -1 (no
    // incoming jigsaw).
    struct Pending { StructurePlacement placement; int remaining; int skip_jigsaw; };
    std::vector<Pending> frontier;
    frontier.push_back({root, max_depth, -1});

    while (!frontier.empty())
    {
        Pending current = frontier.front();
        frontier.erase(frontier.begin());
        if (current.remaining <= 0) continue;

        const StructureTemplate& tmpl = StructureRegistry::getTemplate(current.placement.id);

        for (std::size_t ji = 0; ji < tmpl.jigsaws.size(); ji++)
        {
            if (static_cast<int>(ji) == current.skip_jigsaw) continue;

            const StructureJigsaw& j = tmpl.jigsaws[ji];

            const auto* pool = StructureRegistry::findJigsawPool(j.pool);
            if (!pool || pool->empty()) continue;

            // Per-jigsaw RNG so chain shape doesn't depend on jigsaw ordering
            // across siblings — adding a new jigsaw to a template won't shift
            // the picks already made for the earlier ones.
            uint32_t rng = mix32(current.placement.seed ^ static_cast<uint32_t>(ji) * 2654435761u);

            uint32_t total_w = 0;
            for (const auto& e : *pool) total_w += e.weight;
            if (total_w == 0) continue;

            rng = mix32(rng + 0x9E3779B9u);
            const uint32_t pick = rng % total_w;
            uint32_t acc = 0;
            StructureKey chosen_id;
            for (const auto& e : *pool)
            {
                acc += e.weight;
                if (pick < acc) { chosen_id = e.id; break; }
            }
            if (chosen_id.empty()) continue;

            // Source jigsaw's absolute world facing + position.
            int sx = 0, sy = 0;
            tmpl.toWorld(current.placement.rotation,
                         current.placement.origin_x, current.placement.anchor_y,
                         j.rx, j.ry, sx, sy);
            const StructureJigsawFacing src_facing =
                StructureTemplate::rotateFacing(j.facing, current.placement.rotation);

            // Where the matching target-jigsaw block must land in world coords —
            // one step outward from the source along its facing.
            int target_wx = sx;
            int target_wy = sy;
            StructureJigsawFacing opposite = StructureJigsawFacing::Right;
            switch (src_facing)
            {
                case StructureJigsawFacing::Right: target_wx = sx + 1; opposite = StructureJigsawFacing::Left;  break;
                case StructureJigsawFacing::Left:  target_wx = sx - 1; opposite = StructureJigsawFacing::Right; break;
                case StructureJigsawFacing::Up:    target_wy = sy + 1; opposite = StructureJigsawFacing::Down;  break;
                case StructureJigsawFacing::Down:  target_wy = sy - 1; opposite = StructureJigsawFacing::Up;    break;
            }

            const StructureTemplate& tt = StructureRegistry::getTemplate(chosen_id);
            const StructureRule&     tr = StructureRegistry::getRule(chosen_id);

            // Find a (jigsaw, rotation) pair on the target whose connector
            // matches and whose rotated facing is `opposite`. Rotations are
            // tried in fixed order so the chain stays deterministic.
            const StructureRotation rot_order[] =
            {
                StructureRotation::Original,
                StructureRotation::MirrorX,
                StructureRotation::MirrorY,
                StructureRotation::MirrorXY,
            };

            bool matched = false;
            StructurePlacement next_place{};
            int matched_jigsaw_idx = -1;
            for (StructureRotation rot : rot_order)
            {
                const bool uses_mx = (rot == StructureRotation::MirrorX || rot == StructureRotation::MirrorXY);
                const bool uses_my = (rot == StructureRotation::MirrorY || rot == StructureRotation::MirrorXY);
                if (uses_mx && !tr.allow_mirror)        continue;
                if (uses_my && !tt.allow_mirror_y)      continue;

                for (std::size_t tji = 0; tji < tt.jigsaws.size(); tji++)
                {
                    const StructureJigsaw& tj = tt.jigsaws[tji];
                    if (tj.connector != j.connector) continue;
                    if (StructureTemplate::rotateFacing(tj.facing, rot) != opposite) continue;

                    // Derive origin/anchor so target_world(tj.rx, tj.ry) == (target_wx, target_wy).
                    const int sign_x = uses_mx ? -1 : 1;
                    const int sign_y = uses_my ? -1 : 1;
                    const int origin_x = target_wx - sign_x * (tj.rx - tt.anchor_x);
                    const int anchor_y = target_wy - sign_y * (tj.ry - tt.anchor_y);

                    next_place = StructurePlacement{
                        chosen_id, origin_x, anchor_y, rot,
                        mix32(rng ^ 0xBEEF1234u)
                    };
                    matched = true;
                    matched_jigsaw_idx = static_cast<int>(tji);
                    break;
                }
                if (matched) break;
            }
            if (!matched) continue;

            out.push_back(next_place);
            frontier.push_back({next_place, current.remaining - 1, matched_jigsaw_idx});
        }
    }
}

void NormalWorldGenerator::generateStructures(Chunk& chunk)
{
    const auto& rules = StructureRegistry::rules();
    if (rules.empty()) return;

    const int max_w     = StructureRegistry::maxTemplateWidth();
    const int max_reach = StructureRegistry::maxChainReach();

    // Scan range covers the biggest single stamp (max_w) plus a worst-case
    // chain length (max_reach) on each side. Chained pieces can extend far
    // beyond the root so every neighbour chunk that might host a chain segment
    // still re-resolves the entire chain locally.
    const int reach_blocks = max_w + max_reach;
    const int neighbor_range = (reach_blocks + CHUNK_WIDTH - 1) / CHUNK_WIDTH;

    for (const auto& [id, rule] : rules)
    {
        if (rule.grid <= 0) continue;

        const int chunk_min = chunk.chunk_position * CHUNK_WIDTH;
        const int chunk_max = chunk_min + CHUNK_WIDTH - 1;

        const int search_min = chunk_min - neighbor_range * CHUNK_WIDTH;
        const int search_max = chunk_max + neighbor_range * CHUNK_WIDTH;

        const int grid_min = floorDiv(search_min, rule.grid);
        const int grid_max = floorDiv(search_max, rule.grid);

        for (int gx = grid_min; gx <= grid_max; gx++)
        {
            for (const StructurePlacement& p : getStructurePlacementsInCell(gx, id))
            {
                StructureRegistry::stamp(chunk, p.id, p.origin_x, p.anchor_y, p.rotation, p.seed);
            }
        }
    }
}
