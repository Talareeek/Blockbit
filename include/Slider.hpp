#ifndef SLIDER_HPP
#define SLIDER_HPP

#include <string>

#include "UIElement.hpp"

class Slider : public UIElement
{
private:

    std::string label;

    float value = 0.0f;
    float min = 0.0f;
    float max = 1.0f;

    unsigned precision = 0;

    bool dragging = false;

public:

    Slider() = default;
    Slider(sf::Vector2f position, sf::Vector2f size, float min, float max, float value, std::string label = "");
    Slider(UIElement::ScreenRelative screen_relative, float min, float max, float value, std::string label = "");

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    float getValue() const;
    void setValue(float value);

    void setLabel(const std::string& label);
    const std::string& getLabel() const;

    void setPrecision(unsigned digits);
    unsigned getPrecision() const;
};

#endif // SLIDER_HPP
