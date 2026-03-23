#ifndef UI_ELEMENT_HPP
#define UI_ELEMENT_HPP

#include <SFML/Graphics.hpp>

class UIElement
{
public:

    struct ScreenRelative
    {
        sf::Vector2f position;
        sf::Vector2f size;

        bool keep_aspect = false;

        enum class Axis
        {
            X,
            Y
        } axis = Axis::X;
    };

protected:

    sf::Vector2f position;
    sf::Vector2f size;

    ScreenRelative screen_relative;
    bool is_screen_relative = false;

public:

    UIElement(const sf::Vector2f& position, const sf::Vector2f& size) : position(position), size(size) {}
    UIElement(const ScreenRelative& screen_relative) : screen_relative(screen_relative), is_screen_relative(true) {}
    UIElement(const ScreenRelative&& screen_relative) : screen_relative(screen_relative), is_screen_relative(true) {}
    virtual ~UIElement() = default;

    virtual void handleEvent(const sf::Event& event) = 0;
    virtual void update(float dt) = 0;
    virtual void render(sf::RenderWindow& window) = 0;

    void setPosition(sf::Vector2f pos);

    sf::Vector2f getPosition() const;

    void setSize(sf::Vector2f s);

    sf::Vector2f getSize() const;

    void updateScreenRelative(const sf::Vector2u& windowSize);
};

extern sf::Vector2f computeAbsolutePosition(const UIElement::ScreenRelative& sr, const sf::Vector2u& windowSize);

extern sf::Vector2f computeAbsoluteSize(const UIElement::ScreenRelative& sr, const sf::Vector2u& windowSize);


#endif // UI_ELEMENT_HPP