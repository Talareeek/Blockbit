#ifndef LABEL_HPP
#define LABEL_HPP

#include "UIElement.hpp"
#include <string>

class Label : public UIElement
{
private:

    std::string text;
    sf::Color color = sf::Color::White;

public:

    using UIElement::UIElement;

    void setText(const std::string& text);

    const std::string& getText() const;

    void setColor(const sf::Color& color);
    const sf::Color& getColor() const;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};


class WLabel : public UIElement
{
private:

    std::wstring text;
    sf::Color color = sf::Color::White;

public:

    using UIElement::UIElement;

    void setText(const std::wstring& text);

    const std::wstring& getText() const;

    void setColor(const sf::Color& color);
    const sf::Color& getColor() const;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // LABEL_HPP