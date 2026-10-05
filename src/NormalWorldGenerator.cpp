#include "../include/NormalWorldGenerator.hpp"

NormalWorldGenerator::NormalWorldGenerator(unsigned int seed, NormalWorldGeneratorProperties properties) : seed{seed}, perlin{seed}, properties{properties}
{

}

Climate NormalWorldGenerator::getClimate(int x) const
{
    Climate climate;

    auto extend = [](float value)
    {
        return value * 2 - 1.0f;
    };

    climate.temperature = extend(perlin.noise(x * 0.008f, 1000));

    climate.humidity = extend(perlin.noise(x * 0.008f, 2000));

    climate.continentalness = extend(perlin.noise(x * 0.003f, 3000));

    climate.erosion = perlin.noise(x * 0.015f, 4000);

    climate.weirdness = perlin.noise(x * 0.020f, 5000);

    return climate;
}

Biome NormalWorldGenerator::getBiome(int x) const
{
    auto c = getClimate(x);

    if (c.continentalness < -0.5) return Biome::Ocean;

    if (c.temperature < -0.4) return Biome::Snow;

    if (c.weirdness > 0.6) return Biome::Mountains;

    if (c.temperature > 0.4f)
    {
        if (c.humidity > 0) return Biome::Savanna;
        else return Biome::Desert;
    }

    return Biome::Plains;
}

Chunk NormalWorldGenerator::generateChunk(int chunk_position)
{
    Chunk chunk;
    chunk.chunk_position = chunk_position;

    generateTerrain(chunk);
    generateCaves(chunk);
    //generateOres(chunk);
    //generateNature(chunk_position);

    chunk.generated = true;
    chunk.dirty = true;
    chunk.meshDirty = true;

    return chunk;
}

void NormalWorldGenerator::generateTerrain(Chunk& chunk)
{
    for (int x = 0; x < CHUNK_WIDTH; x++)
    {
        int world_x = chunk.chunk_position * CHUNK_WIDTH + x;
        int height = getHeight(world_x);

        Climate climate = getClimate(world_x);
        Biome biome = getBiome(world_x);

        chunk.climates[x] = climate;
        chunk.biomes[x] = biome;

        Block primary_block = surface_blocks[biome].first;
        Block secondary_block = surface_blocks[biome].second;

        for (int y = 0; y < CHUNK_HEIGHT; y++)
        {
            if (y == 0)
                chunk.blocks[y][x] = {BlockID::Bedrock, 0};

            else if (y < height - 4)
                chunk.blocks[y][x] = {BlockID::Stone, 0};

            else if (y < height - 1)
                chunk.blocks[y][x] = secondary_block;

            else if (y == height - 1)
            {
                if (y < SEA_LEVEL - 1) chunk.blocks[y][x] = secondary_block;
                else chunk.blocks[y][x] = primary_block;
            }

            else if (y < SEA_LEVEL) chunk.blocks[y][x] = {BlockID::Water, static_cast<uint8_t>(WaterLevel::SOURCE)};

            else chunk.blocks[y][x] = {BlockID::Air, 0};
        }
    }
}

void NormalWorldGenerator::generateCaves(Chunk& chunk)
{
    for (int x = 0; x < CHUNK_WIDTH; x++)
    {
        int world_x = chunk.chunk_position * CHUNK_WIDTH + x;

        for (int y = 5; y < CHUNK_HEIGHT; y++)
        {
            float n = perlin.noise(world_x * 0.05f, y * 0.05f);

            if (n > 0.65f) chunk.blocks[y][x] = {BlockID::Air, 0};
        }
    }
}

/*
void World::generateVein(int x, int y, BlockID ore, int size)
{
    uint32_t seed =
        this->seed ^
        (static_cast<uint32_t>(x) << 16) ^
        static_cast<uint32_t>(y);

    std::mt19937 rng(seed);

    for (int i = 0; i < size; i++)
    {
        if (getBlock(x, y).id == BlockID::Stone)
            setBlock(x, y, {ore, 0});

        x += rng() % 3 - 1;
        y += rng() % 3 - 1;
    }
}

void NormalWorldGenerator::generateOres(Chunk& chunk)
{
    std::mt19937 rng(seed + chunk.chunk_position);

    int ruby_height = rng() % 14 + 1;
    int diamond_height = rng() % 14 + 1;
    int gold_height = rng() % 34 + 16;
    int iron_height = rng() % 204 + 51;

    int x = chunk.chunk_position * CHUNK_WIDTH;

    generateVein(x + rng() % CHUNK_WIDTH, diamond_height, BlockID::Diamond_Ore, 8);

    generateVein(x + rng() % CHUNK_WIDTH, gold_height, BlockID::Gold_Ore, 10);

    generateVein(x + rng() % CHUNK_WIDTH, iron_height, BlockID::Iron_Ore, 12);

    generateVein(x + rng() % CHUNK_WIDTH, ruby_height, BlockID::Ruby_Ore, 6);
}

void World::generateTree(
    int x,
    int y,
    int log_height,
    BlockID log_type,
    BlockID leaves_type
)
{
    uint32_t seed =
        this->seed ^
        (static_cast<uint32_t>(x) << 16) ^
        static_cast<uint32_t>(y);

    std::mt19937 rng(seed);

    for (int i = 0; i < log_height; i++)
    {
        setBlock(x, y + i, {log_type, 0});
    }

    int crown_y = y + log_height - 2;

    for (int dx = -2; dx <= 2; dx++)
    {
        for (int dy = -1; dy <= 1; dy++)
        {
            for (int dz = -2; dz <= 2; dz++)
            {
                if (abs(dx) + abs(dy) + abs(dz) <= 3)
                {
                    if (getBlock(
                            x + dx,
                            crown_y + dy
                        ).id == BlockID::Air)
                    {
                        setBlock(
                            x + dx,
                            crown_y + dy,
                            {leaves_type, 0}
                        );
                    }
                }
            }
        }
    }
}

void World::generateNature(int chunk_position)
{
    std::mt19937 rng(seed + chunk_position * 31);

    bool tree_spawns = rng() % 100 <= 35;

    if (tree_spawns)
    {
        int x =
            chunk_position * CHUNK_WIDTH +
            rng() % CHUNK_WIDTH;

        int y = CHUNK_HEIGHT - 1;

        while (
            getBlock(x, y).id != BlockID::Grass &&
            y > 0
        )
        {
            y--;
        }

        if (getBlock(x, y).id == BlockID::Grass)
        {
            generateTree(
                x,
                y,
                rng() % 5 + 3,
                BlockID::Oak_Log,
                BlockID::Oak_Leaves
            );
        }
    }
}
*/

double NormalWorldGenerator::getHeightNoise(double x) const
{
    double total = 0;

    double amplitude = static_cast<double>(properties.amplitude);

    double frequency = static_cast<double>(properties.frequency);

    for (int i = 0; i < 4; i++)
    {
        total += perlin.noise(x * frequency, 0.0) * amplitude;

        amplitude *= static_cast<double>(properties.persistence);

        frequency *= 2.0f;
    }

    return total;
}

int NormalWorldGenerator::getHeight(int worldX) const
{
    Climate climate = getClimate(worldX);

    double x = static_cast<float>(worldX);

    float base = properties.base_height + climate.continentalness * 30.0f;

    float scale = properties.height_scale * (1.0f - climate.erosion * 0.35f);

    float terrain = getHeightNoise(static_cast<double>(worldX));

    float ridge = (1.0f - std::abs(perlin.noise(worldX * 0.003, 9000) * 2.0f - 1.0f)) * climate.weirdness * 20.0f;

    return static_cast<int>(base + terrain * scale + ridge);
}