#include "../include/SkygridWorldGenerator.hpp"
#include "../include/Block.hpp"

#include <algorithm>

SkygridWorldGenerator::SkygridWorldGenerator(unsigned int seed) : seed(seed) {}

uint32_t SkygridWorldGenerator::cellHash(int32_t x, int32_t y) const
{
    uint32_t h = seed ^ 0x9E3779B9u;
    h ^= static_cast<uint32_t>(x) * 374761393u;
    h = (h << 13) | (h >> 19);
    h ^= static_cast<uint32_t>(y) * 668265263u;
    h = (h << 17) | (h >> 15);
    h *= 2654435761u;
    h ^= h >> 16;
    return h;
}

static BlockKey pickBlock(uint32_t h)
{
    static const std::vector<BlockKey> ids = []
    {
        std::vector<BlockKey> keys;
        keys.reserve(blockDatabase.size());
        for (const auto& [key, _] : blockDatabase) keys.push_back(key);
        std::sort(keys.begin(), keys.end());
        return keys;
    }();
    return ids[h % ids.size()];
}

Chunk SkygridWorldGenerator::generateChunk(int chunk_position)
{
    Chunk chunk;
    chunk.chunk_position = chunk_position;
    chunk.generated = true;
    chunk.dirty = true;
    chunk.meshDirty = true;

    for (int x = 0; x < CHUNK_WIDTH; ++x)
    {
        chunk.climates[x] = Climate{0.0, 0.0, 0.0, 0.0, 0.0};
        chunk.biomes[x] = Biome::Plains;
    }

    for (int y = 0; y < CHUNK_HEIGHT; ++y)
    {
        for (int x = 0; x < CHUNK_WIDTH; ++x)
        {
            chunk.blocks[y][x] = {Blocks::Air, 0};
        }
    }

    if (blockDatabase.empty()) return chunk;

    for (int y = 0; y < CHUNK_HEIGHT; y += GRID_STEP)
    {
        for (int lx = 0; lx < CHUNK_WIDTH; ++lx)
        {
            int wx = chunk_position * CHUNK_WIDTH + lx;
            if (((wx % GRID_STEP) + GRID_STEP) % GRID_STEP != 0) continue;

            chunk.blocks[y][lx] = {pickBlock(cellHash(wx, y)), 0};
        }
    }

    return chunk;
}
