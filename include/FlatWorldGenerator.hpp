#ifndef FLAT_WORLD_GENERATOR_HPP
#define FLAT_WORLD_GENERATOR_HPP

#include "WorldGenerator.hpp"
#include "Block.hpp"

#include <vector>

struct FlatWorldLayer
{
    BlockID block;
    int count;
};

struct FlatWorldProperties
{
    std::vector<FlatWorldLayer> layers;

    static FlatWorldProperties defaults()
    {
        return FlatWorldProperties
        {
            {
                {BlockID::Bedrock, 1},
                {BlockID::Stone,   4},
                {BlockID::Dirt,    4},
                {BlockID::Grass,   1}
            }
        };
    }
};

class FlatWorldGenerator : public WorldGenerator
{
private:

    FlatWorldProperties properties;

public:

    FlatWorldGenerator() : properties(FlatWorldProperties::defaults()) {}
    FlatWorldGenerator(FlatWorldProperties properties) : properties(std::move(properties)) {}

    Chunk generateChunk(int chunk_position) override;
};

#endif // FLAT_WORLD_GENERATOR_HPP
