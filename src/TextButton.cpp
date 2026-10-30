#include "../include/TextButton.hpp"

#include "../include/AssetManager.hpp"
#include "../include/Settings.hpp"

#include <algorithm>

TextButton::TextButton() : UIElement({0.0f, 0.0f}, {100.0f, 24.0f})
{
    this->text = "Option";
    this->hoverColor = Settings::ui.main_color;
    this->onClick = [](){};
}

TextButton::TextButton(const UIElement::ScreenRelative& screenRelative, const std::string& text, std::function<void()> onClick) : UIElement(screenRelative)
{
    this->text = text;
    this->hoverColor = Settings::ui.main_color;
    this->onClick = std::move(onClick);
}

void TextButton::setColors(sf::Color idle, sf::Color hovered)
{
    idleColor = idle;
    hoverColor = hovered;
}

void TextButton::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::MouseButtonPressed>())
    {
        if(event.getIf<sf::Event::MouseButtonPressed>()->button == sf::Mouse::Button::Left)
        {
            sf::IntRect rect({{static_cast<int>(position.x), static_cast<int>(position.y)}, {static_cast<int>(size.x), static_cast<int>(size.y)}});
            sf::Vector2i mousePos = event.getIf<sf::Event::MouseButtonPressed>()->position;

            if(rect.contains(mousePos))
            {
                sound.setVolume(100);
                sound.play();
                onClick();
            }
        }
    }
    else if (event.getIf<sf::Event::MouseMoved>())
    {
        sf::IntRect rect({{static_cast<int>(position.x), static_cast<int>(position.y)}, {static_cast<int>(size.x), static_cast<int>(size.y)}});
        sf::Vector2i mousePos = event.getIf<sf::Event::MouseMoved>()->position;
        hover = rect.contains(mousePos);
    }
    else if(event.is<sf::Event::TouchBegan>())
    {
        sf::IntRect rect({{static_cast<int>(position.x), static_cast<int>(position.y)}, {static_cast<int>(size.x), static_cast<int>(size.y)}});
        sf::Vector2i touch_position = event.getIf<sf::Event::TouchBegan>()->position;
        hover = rect.contains(touch_position);
    }
    else if(event.is<sf::Event::TouchMoved>())
    {
        sf::IntRect rect({{static_cast<int>(position.x), static_cast<int>(position.y)}, {static_cast<int>(size.x), static_cast<int>(size.y)}});
        sf::Vector2i touch_position = event.getIf<sf::Event::TouchMoved>()->position;
        if(!rect.contains(touch_position)) hover = false;
    }
    else if(event.is<sf::Event::TouchEnded>())
    {
        sf::IntRect rect({{static_cast<int>(position.x), static_cast<int>(position.y)}, {static_cast<int>(size.x), static_cast<int>(size.y)}});
        sf::Vector2i touch_position = event.getIf<sf::Event::TouchEnded>()->position;
        if(rect.contains(touch_position))
        {
            sound.setVolume(100);
            sound.play();
            onClick();
        }
    }
}

void TextButton::update(float dt)
{
    float target = hover ? 1.0f : 0.0f;
    hoverT += (target - hoverT) * std::min(1.0f, dt * 12.0f);
}

void TextButton::render(sf::RenderWindow& window)
{
    sf::Color color
    (
        static_cast<std::uint8_t>(idleColor.r + (hoverColor.r - idleColor.r) * hoverT),
        static_cast<std::uint8_t>(idleColor.g + (hoverColor.g - idleColor.g) * hoverT),
        static_cast<std::uint8_t>(idleColor.b + (hoverColor.b - idleColor.b) * hoverT)
    );

    unsigned int characterSize = static_cast<unsigned int>(size.y * 0.65f);
    sf::Text textObj(AssetManager::getFont(AssetManager::FontID::PressStart2P), text, characterSize);

    sf::FloatRect bounds = textObj.getLocalBounds();
    float textScale = 1.0f;

    if(bounds.size.x > 0 && bounds.size.y > 0)
    {
        float scaleX = (size.x * 0.95f) / bounds.size.x;
        float scaleY = (size.y * 0.75f) / bounds.size.y;
        textScale = std::min(scaleX, scaleY);
    }

    float indent = size.x * 0.08f * hoverT;

    textObj.setOrigin({bounds.position.x, bounds.position.y + bounds.size.y / 2.0f});
    textObj.setPosition({position.x + indent, position.y + size.y / 2.0f});
    textObj.setScale({textScale, textScale});
    textObj.setFillColor(color);
    textObj.setOutlineThickness(1.0f);
    textObj.setOutlineColor(sf::Color(0, 0, 0, 180));

    window.draw(textObj);
}
