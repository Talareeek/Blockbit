#include "../include/NoodleWorldGenerator.hpp"
#include "../include/Block.hpp"

NoodleWorldGenerator::NoodleWorldGenerator(unsigned int seed, NormalWorldGeneratorProperties surface_properties, double threshold)
    : perlin{seed ^ 0x51EDE2A1u},
      surface{seed, surface_properties},
      threshold{threshold}
{
}

double NoodleWorldGenerator::fbm(double x, double y) const
{
    double total = 0.0;
    double amplitude = 1.0;
    double frequency = 1.0;
    double norm = 0.0;

    for (int i = 0; i < OCTAVES; i++)
    {
        total += perlin.noise(x * frequency, y * frequency + i * 31.7) * amplitude;
        norm += amplitude;

        amplitude *= 0.5;
        frequency *= 2.0;
    }

    return total / norm;
}

bool NoodleWorldGenerator::isNoodle(int world_x, int y) const
{
    const double n = fbm(world_x * FREQ, y * FREQ);

    return n > threshold;
}

Chunk NoodleWorldGenerator::generateChunk(int chunk_position)
{
    Chunk chunk;

    chunk.chunk_position = chunk_position;

    for (int x = 0; x < CHUNK_WIDTH; x++)
    {
        const int world_x = chunk_position * CHUNK_WIDTH + x;

        const Climate climate = surface.getClimate(world_x);
        const Biome biome = surface.getBiome(world_x);

        chunk.climates[x] = climate;
        chunk.biomes[x] = biome;

        const auto& pair = surface_blocks.at(biome);
        const Block top_block = pair.first;
        const Block sub_block = pair.second;

        for (int y = 0; y < CHUNK_HEIGHT; y++)
        {
            if (!isNoodle(world_x, y))
            {
                chunk.blocks[y][x] = {BlockID::Air, 0};
                continue;
            }

            // Solid noodle cell — pick grass/dirt/sand/... depending on how
            // many cells above us are solid, so strand tops read like a
            // normal-terrain surface.
            int depth = 4;

            for (int dy = 1; dy <= 4; dy++)
            {
                const int probe_y = y + dy;

                if (probe_y >= CHUNK_HEIGHT || !isNoodle(world_x, probe_y))
                {
                    depth = dy - 1;
                    break;
                }
            }

            if (depth == 0)
            {
                chunk.blocks[y][x] = top_block;
            }
            else if (depth <= 3)
            {
                chunk.blocks[y][x] = sub_block;
            }
            else
            {
                chunk.blocks[y][x] = {BlockID::Stone, 0};
            }
        }
    }

    chunk.generated = true;
    chunk.dirty = true;
    chunk.meshDirty = true;

    return chunk;
}
