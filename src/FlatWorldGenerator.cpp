#include "../include/FlatWorldGenerator.hpp"

Chunk FlatWorldGenerator::generateChunk(int chunk_position)
{
    Chunk chunk;
    chunk.chunk_position = chunk_position;

    chunk.dirty = true;
    chunk.meshDirty = true;
    chunk.generated = true;

    for (int y = 0; y < CHUNK_HEIGHT; ++y)
    {
        for (int x = 0; x < CHUNK_WIDTH; ++x)
        {
            if (y == 0)
                chunk.blocks[y][x] = {BlockID::Bedrock, 0};
            else if (y < 5)
                chunk.blocks[y][x] = {BlockID::Stone, 0};
            else if (y == 5)
                chunk.blocks[y][x] = {BlockID::Dirt, 0};
            else if (y == 6)
                chunk.blocks[y][x] = {BlockID::Grass, 0};
            else
                chunk.blocks[y][x] = {BlockID::Air, 0};
        }
    }

    return chunk;
}