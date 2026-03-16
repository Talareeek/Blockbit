#ifndef BUTTON_HPP
#define BUTTON_HPP

#include <string>
#include <functional>

#include "UIElement.hpp"

class Button : public UIElement
{
private:
    std::string text;
    sf::Color color;
    std::function<void()> onClick;

    bool cursorHovering = false;

    sf::Cursor cursor{sf::Cursor::Type::Arrow};

public:
    Button();
    Button(const sf::Vector2f& position, const sf::Vector2f& size, sf::Color color, const std::string& text, std::function<void()> onClick = [](){});

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // BUTTON_HPP