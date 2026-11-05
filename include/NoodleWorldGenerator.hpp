#ifndef NOODLE_WORLD_GENERATOR_HPP
#define NOODLE_WORLD_GENERATOR_HPP

#include "WorldGenerator.hpp"
#include "NormalWorldGenerator.hpp"
#include "PerlinNoise.hpp"

class NoodleWorldGenerator : public WorldGenerator
{
private:

    PerlinNoise perlin;
    NormalWorldGenerator surface;
    double threshold;

    double fbm(double x, double y) const;
    bool isNoodle(int world_x, int y) const;

public:

    NoodleWorldGenerator(unsigned int seed, NormalWorldGeneratorProperties surface_properties, double threshold);

    Chunk generateChunk(int chunk_position) override;

    static constexpr double FREQ = 0.035;

    static constexpr int OCTAVES = 3;

    // PerlinNoise::noise returns values in [0, 1] with mean ~0.5. A threshold
    // just above the mean keeps ~a third of cells solid; higher trims the
    // strands thinner, lower fattens them. 0.55 is a good branching default.
    static constexpr double DEFAULT_THRESHOLD = 0.55;
};

#endif // NOODLE_WORLD_GENERATOR_HPP
