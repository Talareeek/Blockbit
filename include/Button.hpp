#ifndef BUTTON_HPP
#define BUTTON_HPP

#include <string>
#include <functional>

#include "UIElement.hpp"
#include "AssetManager.hpp"

#include <SFML/Audio.hpp>

class Button : public UIElement
{
private:
    std::string text;
    sf::Color color;
    std::function<void()> onClick;

    bool Clicked = false;

    sf::Sound sound{AssetManager::getSound(AssetManager::SoundID::Click)};

    bool hover = false;

    float scale = 1.0f;

public:

    Button();
    Button(const sf::Vector2f& position, const sf::Vector2f& size, sf::Color color, const std::string& text, std::function<void()> onClick = [](){});
    Button(const UIElement::ScreenRelative& screenRelative, sf::Color color, const std::string& text, std::function<void()> onClick = [](){});

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    void setText(std::string text);

    bool clicked();
};

#endif // BUTTON_HPP