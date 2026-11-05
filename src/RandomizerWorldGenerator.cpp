#include "../include/RandomizerWorldGenerator.hpp"
#include "../include/Block.hpp"

#include <iterator>

RandomizerWorldGenerator::RandomizerWorldGenerator(unsigned int seed) : seed(seed) {}

uint32_t RandomizerWorldGenerator::cellHash(int32_t x, int32_t y) const
{
    uint32_t h = seed ^ 0xA2C2A6B3u;
    h ^= static_cast<uint32_t>(x) * 374761393u;
    h = (h << 13) | (h >> 19);
    h ^= static_cast<uint32_t>(y) * 668265263u;
    h = (h << 17) | (h >> 15);
    h *= 2654435761u;
    h ^= h >> 16;
    return h;
}

static BlockID pickBlock(uint32_t h)
{
    BlockID candidate = static_cast<BlockID>(h % blockDatabase.size());
    if (blockDatabase.find(candidate) == blockDatabase.end())
    {
        auto it = blockDatabase.begin();
        std::advance(it, h % blockDatabase.size());
        candidate = it->first;
    }
    return candidate;
}

Chunk RandomizerWorldGenerator::generateChunk(int chunk_position)
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

    if (blockDatabase.empty())
    {
        for (int y = 0; y < CHUNK_HEIGHT; ++y)
        {
            for (int x = 0; x < CHUNK_WIDTH; ++x)
            {
                chunk.blocks[y][x] = {BlockID::Air, 0};
            }
        }
        return chunk;
    }

    for (int y = 0; y < CHUNK_HEIGHT; ++y)
    {
        for (int lx = 0; lx < CHUNK_WIDTH; ++lx)
        {
            int wx = chunk_position * CHUNK_WIDTH + lx;
            chunk.blocks[y][lx] = {pickBlock(cellHash(wx, y)), 0};
        }
    }

    return chunk;
}
