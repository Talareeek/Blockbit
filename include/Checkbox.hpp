#ifndef CHECKBOX_HPP
#define CHECKBOX_HPP

#include <string>

#include "UIElement.hpp"

class Checkbox : public UIElement
{
private:

    std::string label;

    bool value = false;

public:

    Checkbox() = default;
    Checkbox(sf::Vector2f position, sf::Vector2f size, bool value = false, std::string label = "");
    Checkbox(UIElement::ScreenRelative screen_relative, bool value = false, std::string label = "");

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    bool getValue() const;
    void setValue(bool value);

    void setLabel(const std::string& label);
    const std::string& getLabel() const;
};

#endif // CHECKBOX_HPP
