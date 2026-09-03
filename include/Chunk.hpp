#ifndef CHUNK_HPP
#define CHUNK_HPP

#include "Block.hpp"
#include "Climate.hpp"
#include "UUID.hpp"

#include <SFML/System/Vector2.hpp>
#include <vector>
#include <cstdint>
#include <unordered_set>

constexpr int CHUNK_WIDTH = 16;
constexpr int CHUNK_HEIGHT = 256;

struct Chunk
{
    int chunk_position;

    std::unordered_set<UUID> entity_ids;

    Block blocks[CHUNK_HEIGHT][CHUNK_WIDTH];

    Climate climates[CHUNK_WIDTH];
    Biome biomes[CHUNK_WIDTH];

    bool dirty;
    bool generated;
    bool meshDirty;
};

#endif // CHUNK_HPP