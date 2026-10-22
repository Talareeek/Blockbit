#ifndef CLIMATE_HPP
#define CLIMATE_HPP

struct Climate
{
    double temperature;
    double humidity;
    double continentalness;
    double erosion;
    double weirdness;
};

enum class Biome
{
    Plains,
    Forest,
    Ocean,
    Desert,
    Savanna,
    Mountains,
    Snow,
    Jungle,
    Taiga,
    SnowyTaiga,
    GiantTaiga
};

#endif // CLIMATE_HPP