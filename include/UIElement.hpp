#ifndef UI_ELEMENT_HPP
#define UI_ELEMENT_HPP

#include <SFML/Graphics.hpp>

class UIElement
{
protected:
    sf::Vector2f position;
    sf::Vector2f size;

public:
    UIElement(const sf::Vector2f& position, const sf::Vector2f& size) : position(position), size(size) {}
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
};

#endif // UI_ELEMENT_HPP