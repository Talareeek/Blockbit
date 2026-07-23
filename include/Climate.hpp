#ifndef CLIMATE_HPP
#define CLIMATE_HPP

struct Climate
{
    float temperature;
    float humidity;
    float continentalness;
    float erosion;
    float weirdness;
};

enum class Biome
{
    Plains,
    Forest,
    Ocean,
    Desert,
    Savanna,
    Mountains,
    Snow
};

#endif // CLIMATE_HPP