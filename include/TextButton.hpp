#ifndef TEXT_BUTTON_HPP
#define TEXT_BUTTON_HPP

#include <string>
#include <functional>

#include "UIElement.hpp"
#include "AssetManager.hpp"

#include <SFML/Audio.hpp>

class TextButton : public UIElement
{
private:
    std::string text;
    sf::Color idleColor{200, 200, 200};
    sf::Color hoverColor;
    std::function<void()> onClick;

    sf::Sound sound{AssetManager::getSound(AssetManager::SoundID::Click)};

    bool hover = false;
    float hoverT = 0.0f;

public:

    TextButton();
    TextButton(const UIElement::ScreenRelative& screenRelative, const std::string& text, std::function<void()> onClick = [](){});

    void setColors(sf::Color idle, sf::Color hovered);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // TEXT_BUTTON_HPP
