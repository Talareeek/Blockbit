#include "../include/Checkbox.hpp"
#include "../include/AssetManager.hpp"

namespace
{
    constexpr float BOX_SIZE_RATIO = 0.9f;
    constexpr float BOX_PADDING_RATIO = 0.1f;
}

Checkbox::Checkbox(sf::Vector2f position, sf::Vector2f size, bool value, std::string label)
    : UIElement(position, size), label(std::move(label)), value(value) {}

Checkbox::Checkbox(UIElement::ScreenRelative screen_relative, bool value, std::string label)
    : UIElement(screen_relative), label(std::move(label)), value(value) {}

bool Checkbox::getValue() const
{
    return value;
}

void Checkbox::setValue(bool v)
{
    value = v;
}

void Checkbox::setLabel(const std::string& l)
{
    label = l;
}

const std::string& Checkbox::getLabel() const
{
    return label;
}

void Checkbox::handleEvent(const sf::Event& event)
{
    if (auto* m = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (m->button == sf::Mouse::Button::Left)
        {
            float box_side = size.y * BOX_SIZE_RATIO;
            float box_y = position.y + (size.y - box_side) * 0.5f;

            sf::FloatRect box_rect({ position.x, box_y }, { box_side, box_side });

            sf::Vector2f mp{ static_cast<float>(m->position.x), static_cast<float>(m->position.y) };

            if (box_rect.contains(mp))
            {
                value = !value;
            }
        }
    }
}

void Checkbox::update(float /*dt*/)
{

}

void Checkbox::render(sf::RenderWindow& window)
{
    float box_side = size.y * BOX_SIZE_RATIO;
    float box_y = position.y + (size.y - box_side) * 0.5f;

    sf::RectangleShape box({ box_side, box_side });
    box.setPosition({ position.x, box_y });
    box.setFillColor(sf::Color(60, 60, 60));
    box.setOutlineThickness(1.5f);
    box.setOutlineColor(sf::Color(20, 20, 20));
    window.draw(box);

    if (value)
    {
        float pad = box_side * BOX_PADDING_RATIO;
        sf::RectangleShape check({ box_side - 2.0f * pad, box_side - 2.0f * pad });
        check.setPosition({ position.x + pad, box_y + pad });
        check.setFillColor(sf::Color(120, 180, 240));
        window.draw(check);
    }

    if (!label.empty())
    {
        unsigned font_size = static_cast<unsigned>(box_side * 0.75f);
        if (font_size == 0) font_size = 1;

        sf::Text label_text(AssetManager::getFont(AssetManager::FontID::PressStart2P), label, font_size);
        label_text.setFillColor(sf::Color::White);
        label_text.setOutlineThickness(1.0f);
        label_text.setOutlineColor(sf::Color::Black);

        auto bounds = label_text.getLocalBounds();
        label_text.setOrigin({ bounds.position.x, bounds.position.y + bounds.size.y * 0.5f });
        label_text.setPosition({ position.x + box_side + size.x * 0.02f, box_y + box_side * 0.5f });

        window.draw(label_text);
    }
}
