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
            chunk.blocks[y][x] = {BlockID::Air, 0};
        }
    }

    int y = 0;
    for (const FlatWorldLayer& layer : properties.layers)
    {
        if (layer.count <= 0) continue;

        int top = std::min(CHUNK_HEIGHT, y + layer.count);

        for (int ly = y; ly < top; ++ly)
        {
            for (int x = 0; x < CHUNK_WIDTH; ++x)
            {
                chunk.blocks[ly][x] = {layer.block, 0};
            }
        }

        y = top;
        if (y >= CHUNK_HEIGHT) break;
    }

    return chunk;
}
