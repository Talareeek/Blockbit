#ifndef FLAT_WORLD_GENERATOR_HPP
#define FLAT_WORLD_GENERATOR_HPP

#include "WorldGenerator.hpp"
#include "Block.hpp"
#include "BBT.hpp"

#include <vector>

struct FlatWorldLayer
{
    BlockKey block;
    int count;

    Tag serialize() const;
    static FlatWorldLayer deserialize(const Tag& tag);
};

struct FlatWorldProperties
{
    std::vector<FlatWorldLayer> layers;

    static FlatWorldProperties defaults()
    {
        return FlatWorldProperties
        {
            {
                {Blocks::Bedrock, 1},
                {Blocks::Stone,   4},
                {Blocks::Dirt,    4},
                {Blocks::Grass,   1}
            }
        };
    }

    Tag serialize() const;
    void deserialize(const Tag& tag);
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
