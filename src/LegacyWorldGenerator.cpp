#include "../include/LegacyWorldGenerator.hpp"

#include <algorithm>
#include <cstdint>
#include <random>

LegacyWorldGenerator::LegacyWorldGenerator(unsigned int seed) : seed{seed}, perlin(seed)
{
}

Chunk LegacyWorldGenerator::generateChunk(int chunk_position)
{
    Chunk chunk;
    chunk.chunk_position = chunk_position;

    generateTerrain(chunk);
    generateCaves(chunk);
    generateOres(chunk);
    generateNature(chunk);

    chunk.generated = true;
    chunk.dirty = true;
    chunk.meshDirty = true;
    return chunk;
}

float LegacyWorldGenerator::getHeightNoise(float x) const
{
    float total = 0;
    float frequency = 0.03f;
    float amplitude = 1.0f;
    float persistence = 0.5f;

    for(int i = 0; i < 4; i++)
    {
        total += perlin.noise(x * frequency, 0.0f) * amplitude;

        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total;
}

int LegacyWorldGenerator::getHeight(int worldX) const
{
    float baseHeight = 45.0f;
    float heightScale = 35.0f;

    return baseHeight + getHeightNoise((float)worldX) * heightScale;
}

void LegacyWorldGenerator::generateTerrain(Chunk& chunk)
{
    for (int x = 0; x < CHUNK_WIDTH; x++)
    {
        const int world_x = chunk.chunk_position * CHUNK_WIDTH + x;
        const int height = std::clamp(getHeight(world_x), 2, CHUNK_HEIGHT - 1);

        for (int y = 0; y < CHUNK_HEIGHT; y++)
        {
            if (y == 0)
                chunk.blocks[y][x] = {Blocks::Bedrock, 0};
            else if (y < height - 4)
                chunk.blocks[y][x] = {Blocks::Stone, 0};
            else if (y < height - 1)
                chunk.blocks[y][x] = {Blocks::Dirt, 0};
            else if (y == height - 1)
                chunk.blocks[y][x] = {height < SEA_LEVEL ? Blocks::Dirt : Blocks::Grass, 0};
            else if (y < SEA_LEVEL)
                chunk.blocks[y][x] = {Blocks::Water, WaterState{WaterState::SOURCE}.pack()};
            else
                chunk.blocks[y][x] = {Blocks::Air, 0};
        }
    }
}

void LegacyWorldGenerator::generateCaves(Chunk& chunk)
{
    for (int x = 0; x < CHUNK_WIDTH; x++)
    {
        const int world_x = chunk.chunk_position * CHUNK_WIDTH + x;
        for (int y = 5; y < CHUNK_HEIGHT; y++)
        {
            const float n = perlin.noise(world_x * 0.05f, y * 0.05f);
            if (n > 0.65f && chunk.blocks[y][x].id == Blocks::Stone)
                chunk.blocks[y][x] = {Blocks::Air, 0};
        }
    }
}

void LegacyWorldGenerator::generateOres(Chunk& chunk)
{
    const std::uint32_t chunk_seed = static_cast<std::uint32_t>(chunk.chunk_position);
    std::mt19937 rng(seed + chunk_seed);

    const auto vein = [&](int start_x, int start_y, BlockKey ore, int size, std::mt19937& random)
    {
        int x = start_x;
        int y = start_y;
        for (int i = 0; i < size; i++)
        {
            if (x >= 0 && x < CHUNK_WIDTH && y > 0 && y < CHUNK_HEIGHT &&
                chunk.blocks[y][x].id == Blocks::Stone)
                chunk.blocks[y][x] = {ore, 0};

            x += static_cast<int>(random() % 3) - 1;
            y += static_cast<int>(random() % 3) - 1;
        }
    };

    const int diamond_y = static_cast<int>(rng() % 14) + 1;
    const int gold_y = static_cast<int>(rng() % 34) + 16;
    const int iron_y = static_cast<int>(rng() % 204) + 51;
    vein(static_cast<int>(rng() % CHUNK_WIDTH), diamond_y, Blocks::Diamond_Ore, 8, rng);
    vein(static_cast<int>(rng() % CHUNK_WIDTH), gold_y, Blocks::Gold_Ore, 10, rng);
    vein(static_cast<int>(rng() % CHUNK_WIDTH), iron_y, Blocks::Iron_Ore, 12, rng);
}

void LegacyWorldGenerator::generateNature(Chunk& chunk)
{
    std::mt19937 rng(seed + static_cast<std::uint32_t>(chunk.chunk_position) * 31u);
    if (rng() % 100 > 35)
        return;

    const int x = static_cast<int>(rng() % CHUNK_WIDTH);
    int surface_y = CHUNK_HEIGHT - 1;
    while (surface_y > 0 && chunk.blocks[surface_y][x].id != Blocks::Grass)
        surface_y--;
    if (surface_y <= 0)
        return;

    const int log_height = static_cast<int>(rng() % 5) + 3;
    for (int dy = 1; dy <= log_height && surface_y + dy < CHUNK_HEIGHT; dy++)
        chunk.blocks[surface_y + dy][x] = {Blocks::Oak_Log, 0};

    const int crown_y = surface_y + log_height - 1;
    for (int dx = -2; dx <= 2; dx++)
    {
        const int leaf_x = x + dx;
        if (leaf_x < 0 || leaf_x >= CHUNK_WIDTH)
            continue;
        for (int dy = -1; dy <= 1; dy++)
        {
            for (int dz = -2; dz <= 2; dz++)
            {
                if (std::abs(dx) + std::abs(dy) + std::abs(dz) > 3)
                    continue;
                const int leaf_y = crown_y + dy;
                if (leaf_y >= 0 && leaf_y < CHUNK_HEIGHT && chunk.blocks[leaf_y][leaf_x].id == Blocks::Air)
                    chunk.blocks[leaf_y][leaf_x] = {Blocks::Oak_Leaves, 0};
            }
        }
    }
}
