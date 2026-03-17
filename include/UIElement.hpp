#ifndef UI_ELEMENT_HPP
#define UI_ELEMENT_HPP

#include <SFML/Graphics.hpp>

class UIElement
{
public:

    struct ScreenRelative
    {
        sf::Vector2f relative_position;
        sf::Vector2f relative_size;
    };

protected:

    sf::Vector2f position;
    sf::Vector2f size;

    ScreenRelative screen_relative;
    bool is_screen_relative = false;

public:

    UIElement(const sf::Vector2f& position, const sf::Vector2f& size) : position(position), size(size) {}
    UIElement(const ScreenRelative& screen_relative) : screen_relative(screen_relative), is_screen_relative(true) {}
    virtual ~UIElement() = default;

    virtual void handleEvent(const sf::Event& event) = 0;
    virtual void update(float dt) = 0;
    virtual void render(sf::RenderWindow& window) = 0;

    void setPosition(sf::Vector2f pos)
    {
        position = pos;
    }

    sf::Vector2f getPosition() const
    {
        return position;
    }

    void setSize(sf::Vector2f s)
    {
        size = s;
    }

    sf::Vector2f getSize() const
    {
        return size;
    }


    void updateScreenRelative(const sf::IntRect& window)
    {
        if (is_screen_relative)
        {
            position.x = screen_relative.relative_position.x * window.size.x;
            position.y = screen_relative.relative_position.y * window.size.y;
            size.x = screen_relative.relative_size.x * window.size.x;
            size.y = screen_relative.relative_size.y * window.size.y;
        }
    }
};

#endif // UI_ELEMENT_HPP