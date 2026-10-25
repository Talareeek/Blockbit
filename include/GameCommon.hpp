#ifndef GAME_COMMON_HPP
#define GAME_COMMON_HPP

#include "UUID.hpp"

#include <cstdint>
#include <SFML/System.hpp>
#include <SFML/Graphics/Color.hpp>
#include <optional>
#include <filesystem>

#include <imgui.h>

class World;
class Entity;
struct TransformComponent;

[[deprecated]] extern Entity& entityWithID(UUID id, World& world);
[[deprecated]] extern bool doesEntityExist(UUID id, World& world);

int positionToChunk(sf::Vector2<double> position);

bool isInRange(TransformComponent& player, TransformComponent& target, float range);
bool isBlockInRange(TransformComponent& player, sf::Vector2i& block, float range);

float shortestDistance(const TransformComponent& a, const TransformComponent& b);

float getTickStep(uint16_t tick_rate);

constexpr float MAX_HIT_DISTANCE = 5.0f;
constexpr float HIT_KNOCKBACK_SCALE = 4.0f;
constexpr float HIT_KNOCKBACK_VERTICAL = 4.0f;
constexpr uint32_t PLAYER_HIT_DAMAGE = 5;

constexpr unsigned int WORLD_UNIT_SIZE_FACTOR = 12;
std::filesystem::path getHomePath();

std::string wstringToString(const std::wstring wstring);

ImVec4 SFMLColorToImGuiColor(const sf::Color color);

struct Hour
{
    uint8_t hours;
    uint8_t minutes;
};

struct Date
{
    uint8_t day;

    enum class Month : uint8_t
    {
        January,
        February,
        March,
        April,
        May,
        June,
        July,
        August,
        September,
        October,
        November,
        December
    } month;

    uint32_t year;
};

Hour daytimeToHour(float daytime);
Date daysToDate(uint64_t days);

std::string dateAndHourString(Date date, Hour hour);

// Seasonal daylight model.
//
// The world's calendar is Gregorian; day length tracks solstices so the
// shortest day is 1 January and the longest is 1 July. The total length of a
// day/night cycle stays fixed at World::DAY_CYCLE_DURATION — only the split
// between daylight and night slides across the year.
constexpr float SHORTEST_DAY_FRACTION = 0.30f;
constexpr float LONGEST_DAY_FRACTION  = 0.70f;

uint16_t dayOfYear(Date date);

// Fraction of a cycle that is daylight. In [SHORTEST_DAY_FRACTION, LONGEST_DAY_FRACTION].
float dayLightFraction(Date date);

// Daylight duration in game-seconds for the given calendar date.
float dayLength(Date date);

// Remap raw dayTime to a normalized phase in [0, 1): 0=midnight, 0.25=sunrise,
// 0.5=noon, 0.75=sunset. Feed this to sky/sun/moon/star/lighting code so
// sunrise and sunset shift with the season but noon/midnight stay anchored.
float solarPhase(float dayTime, uint64_t days);

bool isDaylight(float dayTime, uint64_t days);

#endif // GAME_COMMON_HPP