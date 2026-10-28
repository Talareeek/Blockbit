#include "../include/GameCommon.hpp"

#include "../include/World.hpp"
#include "../include/Entity.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/Settings.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

Entity& entityWithID(UUID id, World& world)
{
    if(world.getEntities().contains(id)) return world.getEntities().at(id);;

    throw std::runtime_error("No entity with UUID: " + id.toString() + " found");
}

bool doesEntityExist(UUID id, World& world)
{
    return world.getEntities().contains(id);
}

int positionToChunk(sf::Vector2<double> position)
{
    return static_cast<int>(std::floor(position.x) / 16);
}

bool isInRange(TransformComponent& player, TransformComponent& target, float range)
{
    return shortestDistance(player, target) <= range;
}

bool isBlockInRange(TransformComponent& player, sf::Vector2i& block, float range)
{
    const double block_min_x = static_cast<double>(block.x);
    const double block_max_x = block_min_x + 1.0;
    const double block_min_y = static_cast<double>(block.y);
    const double block_max_y = block_min_y + 1.0;

    const double player_min_x = player.position.x;
    const double player_max_x = player.position.x + player.size.x;
    const double player_min_y = player.position.y;
    const double player_max_y = player.position.y + player.size.y;

    double dx = 0.0;
    if(block_min_x > player_max_x) dx = block_min_x - player_max_x;
    else if(player_min_x > block_max_x) dx = player_min_x - block_max_x;

    double dy = 0.0;
    if(block_min_y > player_max_y) dy = block_min_y - player_max_y;
    else if(player_min_y > block_max_y) dy = player_min_y - block_max_y;

    return std::sqrt(dx * dx + dy * dy) <= static_cast<double>(range);
}

float shortestDistance(const TransformComponent& a, const TransformComponent& b)
{
    const double a_min_x = a.position.x;
    const double a_max_x = a.position.x + a.size.x;
    const double a_min_y = a.position.y;
    const double a_max_y = a.position.y + a.size.y;

    const double b_min_x = b.position.x;
    const double b_max_x = b.position.x + b.size.x;
    const double b_min_y = b.position.y;
    const double b_max_y = b.position.y + b.size.y;

    double dx = 0.0;
    if(b_min_x > a_max_x) dx = b_min_x - a_max_x;
    else if(a_min_x > b_max_x) dx = a_min_x - b_max_x;

    double dy = 0.0;
    if(b_min_y > a_max_y) dy = b_min_y - a_max_y;
    else if(a_min_y > b_max_y) dy = a_min_y - b_max_y;

    return static_cast<float>(std::sqrt(dx * dx + dy * dy));
}

float getTickStep(uint16_t tick_rate)
{
    return 1.0f / static_cast<float>(tick_rate);
}

std::filesystem::path getHomePath()
{
    std::filesystem::path directory;

    #ifdef _WIN32
        const char* appdata = std::getenv("APPDATA");
        directory = appdata ? std::filesystem::path(appdata) : std::filesystem::temp_directory_path();
    #else
        const char* home = std::getenv("HOME");
        directory = home ? std::filesystem::path(home) : std::filesystem::temp_directory_path();
    #endif

    return directory;
}

std::string wstringToString(const std::wstring wstring)
{
    return std::string(wstring.begin(), wstring.end());
}

ImVec4 SFMLColorToImGuiColor(const sf::Color color)
{
    return ImVec4((float)color.r / 255.0f, (float)color.g / 255.0f, (float)color.b / 255.0f, (float)color.a / 255.0f);
}

#ifndef HEADLESS_BUILD
void applyImGuiTheme()
{
    const sf::Color m = Settings::ui.main_color;
    const float r = static_cast<float>(m.r) / 255.0f;
    const float g = static_cast<float>(m.g) / 255.0f;
    const float b = static_cast<float>(m.b) / 255.0f;

    auto tint = [&](float scale, float alpha) {
        return ImVec4(std::clamp(r * scale, 0.0f, 1.0f),
                      std::clamp(g * scale, 0.0f, 1.0f),
                      std::clamp(b * scale, 0.0f, 1.0f),
                      alpha);
    };

    const ImVec4 accent        = tint(1.00f, 1.00f);
    const ImVec4 accent_dim    = tint(0.55f, 1.00f);
    const ImVec4 accent_bright = tint(1.20f, 1.00f);

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* c = style.Colors;

    c[ImGuiCol_WindowBg]            = ImVec4(0.04f, 0.05f, 0.07f, 0.90f);
    c[ImGuiCol_ChildBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_PopupBg]             = ImVec4(0.06f, 0.07f, 0.10f, 0.95f);
    c[ImGuiCol_Border]              = tint(1.0f, 0.60f);
    c[ImGuiCol_BorderShadow]        = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    c[ImGuiCol_FrameBg]             = tint(0.20f, 0.55f);
    c[ImGuiCol_FrameBgHovered]      = tint(0.45f, 0.65f);
    c[ImGuiCol_FrameBgActive]       = tint(0.65f, 0.75f);

    c[ImGuiCol_TitleBg]             = tint(0.30f, 0.90f);
    c[ImGuiCol_TitleBgActive]       = tint(0.60f, 0.95f);
    c[ImGuiCol_TitleBgCollapsed]    = tint(0.30f, 0.60f);
    c[ImGuiCol_MenuBarBg]           = tint(0.25f, 0.90f);

    c[ImGuiCol_ScrollbarBg]         = ImVec4(0.02f, 0.03f, 0.05f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]       = tint(0.50f, 0.80f);
    c[ImGuiCol_ScrollbarGrabHovered]= tint(0.80f, 0.90f);
    c[ImGuiCol_ScrollbarGrabActive] = accent;

    c[ImGuiCol_CheckMark]           = accent_bright;
    c[ImGuiCol_SliderGrab]          = accent;
    c[ImGuiCol_SliderGrabActive]    = accent_bright;

    c[ImGuiCol_Button]              = tint(0.40f, 0.80f);
    c[ImGuiCol_ButtonHovered]       = accent;
    c[ImGuiCol_ButtonActive]        = tint(0.80f, 1.00f);

    c[ImGuiCol_Header]              = tint(0.45f, 0.70f);
    c[ImGuiCol_HeaderHovered]       = tint(0.75f, 0.85f);
    c[ImGuiCol_HeaderActive]        = accent;

    c[ImGuiCol_Separator]           = tint(0.60f, 0.50f);
    c[ImGuiCol_SeparatorHovered]    = tint(0.90f, 0.80f);
    c[ImGuiCol_SeparatorActive]     = accent;

    c[ImGuiCol_ResizeGrip]          = tint(0.80f, 0.40f);
    c[ImGuiCol_ResizeGripHovered]   = tint(0.90f, 0.80f);
    c[ImGuiCol_ResizeGripActive]    = accent;

    c[ImGuiCol_Tab]                 = tint(0.40f, 0.80f);
    c[ImGuiCol_TabHovered]          = accent;
    c[ImGuiCol_TabActive]           = tint(0.80f, 1.00f);
    c[ImGuiCol_TabUnfocused]        = tint(0.25f, 0.80f);
    c[ImGuiCol_TabUnfocusedActive]  = tint(0.50f, 0.90f);

    c[ImGuiCol_PlotLines]           = accent_dim;
    c[ImGuiCol_PlotLinesHovered]    = accent_bright;
    c[ImGuiCol_PlotHistogram]       = accent;
    c[ImGuiCol_PlotHistogramHovered]= accent_bright;

    c[ImGuiCol_TextSelectedBg]      = tint(1.0f, 0.35f);
    c[ImGuiCol_NavHighlight]        = accent;
    c[ImGuiCol_NavWindowingHighlight]= tint(1.0f, 0.70f);
    c[ImGuiCol_DragDropTarget]      = accent_bright;
}
#endif

Hour daytimeToHour(float daytime)
{
    float passed = daytime / World::DAY_CYCLE_DURATION;

    uint16_t total_minutes = static_cast<uint16_t>(1440.0f * passed);

    return {static_cast<uint8_t>(total_minutes / 60), static_cast<uint8_t>(total_minutes % 60)};
}

constexpr bool isLeapYear(uint32_t year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

constexpr uint16_t daysInYear(uint32_t year)
{
    return isLeapYear(year) ? 366 : 365;
}

constexpr uint8_t daysInMonth(Date::Month month, uint32_t year)
{
    switch (month)
    {
        case Date::Month::January:   return 31;
        case Date::Month::February:  return isLeapYear(year) ? 29 : 28;
        case Date::Month::March:     return 31;
        case Date::Month::April:     return 30;
        case Date::Month::May:       return 31;
        case Date::Month::June:      return 30;
        case Date::Month::July:      return 31;
        case Date::Month::August:    return 31;
        case Date::Month::September: return 30;
        case Date::Month::October:   return 31;
        case Date::Month::November:  return 30;
        case Date::Month::December:  return 31;
    }
    return 30;
}

Date daysToDate(uint64_t days)
{
    constexpr uint64_t DAYS_PER_400_YEARS = 146097;

    uint64_t cycles400 = days / DAYS_PER_400_YEARS;
    uint64_t remainingDays = days % DAYS_PER_400_YEARS;

    uint32_t year = static_cast<uint32_t>(cycles400 * 400);

    while (true)
    {
        uint16_t diy = daysInYear(year);
        if (remainingDays < diy) break;
        remainingDays -= diy;
        ++year;
    }

    Date::Month month = Date::Month::January;

    while (true)
    {
        uint8_t dim = daysInMonth(month, year);
        if (remainingDays < dim) break;
        remainingDays -= dim;
        month = static_cast<Date::Month>(static_cast<uint8_t>(month) + 1);
    }

    Date result;
    result.year = year;
    result.day = static_cast<uint8_t>(remainingDays + 1);
    result.month = month;

    return result;
}

uint16_t dayOfYear(Date date)
{
    uint16_t doy = 0;
    for (uint8_t m = 0; m < static_cast<uint8_t>(date.month); ++m)
    {
        doy += daysInMonth(static_cast<Date::Month>(m), date.year);
    }
    return static_cast<uint16_t>(doy + date.day - 1);
}

float dayLightFraction(Date date)
{
    const uint16_t doy    = dayOfYear(date);
    const uint16_t summer = dayOfYear(Date{1, Date::Month::July, date.year});
    const uint16_t total  = daysInYear(date.year);

    constexpr float PI = 3.14159265358979f;

    // Piecewise so the extremes land exactly on 1 Jan and 1 July despite the
    // two halves of the year being unequal in length.
    float angle;
    if (doy <= summer)
    {
        angle = (static_cast<float>(doy) / static_cast<float>(summer)) * PI;
    }
    else
    {
        const uint16_t winter_half = static_cast<uint16_t>(total - summer);
        angle = PI + (static_cast<float>(doy - summer) / static_cast<float>(winter_half)) * PI;
    }

    constexpr float mean = (SHORTEST_DAY_FRACTION + LONGEST_DAY_FRACTION) * 0.5f;
    constexpr float amp  = (LONGEST_DAY_FRACTION - SHORTEST_DAY_FRACTION) * 0.5f;
    return mean - amp * std::cos(angle);
}

Season seasonOfYear(Date date)
{
    if(date.month > Date::Month::December || (date.month == Date::Month::December && date.day >= 22)) return Season::Winter;
    else if(date.month > Date::Month::September || (date.month == Date::Month::September && date.day >= 23)) return Season::Spring;
    else if(date.month > Date::Month::June || (date.month == Date::Month::June && date.day >= 22)) return Season::Spring;
    else if(date.month > Date::Month::March || (date.month == Date::Month::March && date.day >= 21)) return Season::Spring;
    else return Season::Winter;
}

float dayLength(Date date)
{
    return dayLightFraction(date) * World::DAY_CYCLE_DURATION;
}

float solarPhase(float dayTime, uint64_t days)
{
    const Date  date = daysToDate(days);
    const float frac = dayLightFraction(date);
    const float t    = std::fmod(dayTime / World::DAY_CYCLE_DURATION, 1.0f);

    const float sunrise = 0.5f - frac * 0.5f;
    const float sunset  = 0.5f + frac * 0.5f;

    if (t >= sunrise && t <= sunset)
    {
        return 0.25f + (t - sunrise) / (sunset - sunrise) * 0.5f;
    }

    const float night_len = 1.0f - frac;
    if (t > sunset)
    {
        return 0.75f + (t - sunset) / night_len * 0.5f;
    }
    return std::fmod(0.75f + (1.0f - sunset + t) / night_len * 0.5f, 1.0f);
}

bool isDaylight(float dayTime, uint64_t days)
{
    const Date  date = daysToDate(days);
    const float frac = dayLightFraction(date);
    const float t    = std::fmod(dayTime / World::DAY_CYCLE_DURATION, 1.0f);
    return t >= (0.5f - frac * 0.5f) && t <= (0.5f + frac * 0.5f);
}

std::string dateAndHourString(Date date, Hour hour)
{
    std::ostringstream stream;

    stream << std::setw(2) << std::setfill('0') << static_cast<int>(hour.hours) << ':'
           << std::setw(2) << std::setfill('0') << static_cast<int>(hour.minutes) << "   "
           << static_cast<int>(date.day) << '.'
           << (static_cast<int>(date.month) + 1) << '.'
           << date.year;

    return stream.str();
}