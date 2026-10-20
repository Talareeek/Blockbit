#include "../include/DungeonShooterWorldGenerator.hpp"

DungeonShooterWorldGenerator::DungeonShooterWorldGenerator(unsigned int seed) : seed(seed) {}

uint32_t DungeonShooterWorldGenerator::edgeHash(int32_t a, int32_t b, int32_t direction) const
{
    uint32_t h = seed ^ 0x9E3779B9u;
    h ^= static_cast<uint32_t>(a) * 374761393u;
    h = (h << 13) | (h >> 19);
    h ^= static_cast<uint32_t>(b) * 668265263u;
    h = (h << 17) | (h >> 15);
    h ^= static_cast<uint32_t>(direction) * 2246822519u;
    h ^= h >> 16;
    h *= 2654435761u;
    h ^= h >> 16;
    return h;
}

bool DungeonShooterWorldGenerator::isHorizontalEdgeOpen(int room_x, int room_y) const
{
    if (room_y < 0 || room_y >= ROOMS_PER_CHUNK_Y - 1) return false;
    return (edgeHash(room_x, room_y, 0) & 1u) != 0u;
}

bool DungeonShooterWorldGenerator::isVerticalEdgeOpen(int room_x, int room_y) const
{
    return (edgeHash(room_x, room_y, 1) & 1u) != 0u;
}

Chunk DungeonShooterWorldGenerator::generateChunk(int chunk_position)
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
            chunk.blocks[y][x] = {BlockID::Cobblestone, 0};
        }
    }

    for (int room_slot_x = 0; room_slot_x < ROOMS_PER_CHUNK_X; ++room_slot_x)
    {
        for (int room_y = 0; room_y < ROOMS_PER_CHUNK_Y; ++room_y)
        {
            const int room_x = chunk_position * ROOMS_PER_CHUNK_X + room_slot_x;

            const bool top_open    = isHorizontalEdgeOpen(room_x, room_y);
            const bool bottom_open = isHorizontalEdgeOpen(room_x, room_y - 1);
            const bool right_open  = isVerticalEdgeOpen  (room_x, room_y);
            const bool left_open   = isVerticalEdgeOpen  (room_x - 1, room_y);

            const int base_x = room_slot_x * ROOM_SIZE;
            const int base_y = room_y * ROOM_SIZE;

            for (int ly = 1; ly < ROOM_SIZE - 1; ++ly)
            {
                for (int lx = 1; lx < ROOM_SIZE - 1; ++lx)
                {
                    chunk.blocks[base_y + ly][base_x + lx] = {BlockID::Air, 0};
                }
            }

            if (top_open)
            {
                for (int lx = 1; lx < ROOM_SIZE - 1; ++lx)
                {
                    chunk.blocks[base_y + ROOM_SIZE - 1][base_x + lx] = {BlockID::Air, 0};
                }
            }

            if (bottom_open)
            {
                for (int lx = 1; lx < ROOM_SIZE - 1; ++lx)
                {
                    chunk.blocks[base_y][base_x + lx] = {BlockID::Air, 0};
                }
            }

            if (left_open)
            {
                for (int ly = 1; ly < ROOM_SIZE - 1; ++ly)
                {
                    chunk.blocks[base_y + ly][base_x] = {BlockID::Air, 0};
                }
            }

            if (right_open)
            {
                for (int ly = 1; ly < ROOM_SIZE - 1; ++ly)
                {
                    chunk.blocks[base_y + ly][base_x + ROOM_SIZE - 1] = {BlockID::Air, 0};
                }
            }
        }
    }

    return chunk;
}
