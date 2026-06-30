#include "../include/Slider.hpp"
#include "../include/AssetManager.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace
{
    constexpr float TRACK_WIDTH_RATIO = 0.78f;
    constexpr float KNOB_WIDTH_RATIO  = 0.035f;

    constexpr float LABEL_AREA_RATIO  = 0.45f;
    constexpr float TRACK_AREA_RATIO  = 1.0f - LABEL_AREA_RATIO;

    sf::FloatRect trackRect(const sf::Vector2f& pos, const sf::Vector2f& size)
    {
        float track_w = size.x * TRACK_WIDTH_RATIO;
        float area_h  = size.y * TRACK_AREA_RATIO;
        float track_h = area_h * 0.45f;
        float area_y  = pos.y + size.y * LABEL_AREA_RATIO;
        float track_y = area_y + (area_h - track_h) * 0.5f;
        return { {pos.x, track_y}, {track_w, track_h} };
    }
}

Slider::Slider(sf::Vector2f position, sf::Vector2f size, float min, float max, float value, std::string label)
    : UIElement(position, size), label(std::move(label)), value(value), min(min), max(max) {}

Slider::Slider(UIElement::ScreenRelative screen_relative, float min, float max, float value, std::string label)
    : UIElement(screen_relative), label(std::move(label)), value(value), min(min), max(max) {}

float Slider::getValue() const
{
    return value;
}

void Slider::setValue(float v)
{
    value = std::clamp(v, min, max);
}

void Slider::setLabel(const std::string& l)
{
    label = l;
}

const std::string& Slider::getLabel() const
{
    return label;
}

void Slider::setPrecision(unsigned digits)
{
    precision = digits;
}

unsigned Slider::getPrecision() const
{
    return precision;
}

void Slider::handleEvent(const sf::Event& event)
{
    auto track = trackRect(position, size);

    auto valueFromX = [&](float x)
    {
        float span = track.size.x;
        float t = span > 0.0f ? (x - track.position.x) / span : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);
        return min + t * (max - min);
    };

    if (auto* m = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (m->button == sf::Mouse::Button::Left)
        {
            sf::Vector2f mp{ static_cast<float>(m->position.x), static_cast<float>(m->position.y) };

            float pad = size.x * KNOB_WIDTH_RATIO;
            float area_h = size.y * TRACK_AREA_RATIO;
            sf::FloatRect grab{
                { track.position.x - pad, position.y + size.y * LABEL_AREA_RATIO },
                { track.size.x + 2.0f * pad, area_h }
            };

            if (grab.contains(mp))
            {
                dragging = true;
                value = valueFromX(mp.x);
            }
        }
    }
    else if (auto* m = event.getIf<sf::Event::MouseButtonReleased>())
    {
        if (m->button == sf::Mouse::Button::Left)
        {
            dragging = false;
        }
    }
    else if (auto* m = event.getIf<sf::Event::MouseMoved>())
    {
        if (dragging)
        {
            value = valueFromX(static_cast<float>(m->position.x));
        }
    }
}

void Slider::update(float /*dt*/)
{

}

void Slider::render(sf::RenderWindow& window)
{
    auto track = trackRect(position, size);

    float range = max - min;
    float t = range != 0.0f ? (value - min) / range : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);

    if (!label.empty())
    {
        unsigned label_font_size = static_cast<unsigned>(size.y * LABEL_AREA_RATIO * 0.75f);
        if (label_font_size == 0) label_font_size = 1;

        sf::Text label_text(AssetManager::getFont(0), label, label_font_size);
        label_text.setFillColor(sf::Color::White);
        label_text.setOutlineThickness(1.0f);
        label_text.setOutlineColor(sf::Color::Black);

        auto bounds = label_text.getLocalBounds();
        label_text.setOrigin({ bounds.position.x, bounds.position.y });
        label_text.setPosition({ position.x, position.y + size.y * LABEL_AREA_RATIO * 0.10f });

        window.draw(label_text);
    }

    sf::RectangleShape bg(track.size);
    bg.setPosition(track.position);
    bg.setFillColor(sf::Color(60, 60, 60));
    bg.setOutlineThickness(1.5f);
    bg.setOutlineColor(sf::Color(20, 20, 20));
    window.draw(bg);

    sf::RectangleShape fill({ track.size.x * t, track.size.y });
    fill.setPosition(track.position);
    fill.setFillColor(sf::Color(120, 180, 240));
    window.draw(fill);

    float area_h = size.y * TRACK_AREA_RATIO;
    float knob_w = size.x * KNOB_WIDTH_RATIO * 2.0f;
    float knob_h = area_h * 0.95f;
    float knob_x = track.position.x + track.size.x * t - knob_w * 0.5f;
    float knob_y = position.y + size.y * LABEL_AREA_RATIO + (area_h - knob_h) * 0.5f;

    sf::RectangleShape knob({ knob_w, knob_h });
    knob.setPosition({ knob_x, knob_y });
    knob.setFillColor(dragging ? sf::Color(230, 230, 230) : sf::Color(200, 200, 200));
    knob.setOutlineThickness(1.5f);
    knob.setOutlineColor(sf::Color(30, 30, 30));
    window.draw(knob);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;

    unsigned font_size = static_cast<unsigned>(area_h * 0.75f);
    if (font_size == 0) font_size = 1;

    sf::Text text(AssetManager::getFont(0), ss.str(), font_size);
    text.setFillColor(sf::Color::White);
    text.setOutlineThickness(1.0f);
    text.setOutlineColor(sf::Color::Black);

    auto bounds = text.getLocalBounds();
    text.setOrigin({ bounds.position.x, bounds.position.y + bounds.size.y * 0.5f });

    float value_x = track.position.x + track.size.x + size.x * 0.02f;
    text.setPosition({ value_x, track.position.y + track.size.y * 0.5f });

    window.draw(text);
}
