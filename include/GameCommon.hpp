#ifndef GAME_COMMON_HPP
#define GAME_COMMON_HPP

#include "UUID.hpp"

#include <cstdint>
#include <SFML/System.hpp>
#include <optional>

#include <imgui.h>

class World;
class Entity;
struct TransformComponent;

[[deprecated]] extern Entity& entityWithID(UUID id, World& world);
[[deprecated]] extern bool doesEntityExist(UUID id, World& world);

int positionToChunk(sf::Vector2<double> position);

bool isInRange(TransformComponent& player, TransformComponent& target, float range);
bool isBlockInRange(TransformComponent& player, sf::Vector2i& block, float range);

float getTickStep(uint16_t tick_rate);

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

#endif // GAME_COMMON_HPP