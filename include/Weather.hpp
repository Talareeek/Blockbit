#ifndef WEATHER_HPP
#define WEATHER_HPP

#include "Climate.hpp"

#include <cstdint>

enum class WeatherState : uint8_t
{
    Clear   = 0,
    Rain    = 1,
    Thunder = 2
};

enum class Precipitation : uint8_t
{
    None,
    Rain,
    Snow
};

inline Precipitation precipitationFor(WeatherState state, Biome biome)
{
    if (state == WeatherState::Clear) return Precipitation::None;

    switch (biome)
    {
        case Biome::Desert:
        case Biome::Savanna:
            return Precipitation::None;

        case Biome::Snow:
        case Biome::SnowyTaiga:
        case Biome::Taiga:
        case Biome::GiantTaiga:
            return Precipitation::Snow;

        default:
            return Precipitation::Rain;
    }
}

#endif // WEATHER_HPP
