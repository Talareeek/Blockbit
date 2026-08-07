#ifndef UI_LAYOUT_HPP
#define UI_LAYOUT_HPP

#include "UIElement.hpp"

class UILayout : public UIElement
{
public:

    std::vector<std::unique_ptr<UIElement>> elements;

    float margin;
    float spacing;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

class VerticalStackLayout : public UILayout
{
public:

    void updateToBounds(sf::FloatRect bounds) override;
};

class HorizontalStackLayout : public UILayout
{
public:

    void updateToBounds(sf::FloatRect bounds) override;
};


#endif // UI_LAYOUT_HPP