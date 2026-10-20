#ifndef DUNGEON_SHOOTER_WORLD_GENERATOR_HPP
#define DUNGEON_SHOOTER_WORLD_GENERATOR_HPP

#include "WorldGenerator.hpp"

#include <cstdint>

class DungeonShooterWorldGenerator : public WorldGenerator
{
private:

    unsigned int seed;

    uint32_t edgeHash(int32_t a, int32_t b, int32_t direction) const;
    bool isHorizontalEdgeOpen(int room_x, int room_y) const;
    bool isVerticalEdgeOpen(int room_x, int room_y) const;

public:

    DungeonShooterWorldGenerator(unsigned int seed);

    Chunk generateChunk(int chunk_position) override;

    static constexpr int ROOM_SIZE = 8;
    static constexpr int ROOMS_PER_CHUNK_X = CHUNK_WIDTH / ROOM_SIZE;
    static constexpr int ROOMS_PER_CHUNK_Y = CHUNK_HEIGHT / ROOM_SIZE;
};

#endif // DUNGEON_SHOOTER_WORLD_GENERATOR_HPP
