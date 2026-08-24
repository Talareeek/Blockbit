#ifndef UI_ELEMENT_HPP
#define UI_ELEMENT_HPP

#include <SFML/Graphics.hpp>
#include <optional>

class UIElement
{
public:

    struct ScreenRelative
    {
        enum class ScaleMode
        {
            // x scales with window width, y scales with window height.
            // The element stretches anisotropically to fill the window.
            Stretch,

            // Both axes are scaled by the window's width.
            // The y-axis is centered vertically, so the usable area is a
            // (width x width) square letterboxed top and bottom.
            UniformByWidth,

            // Both axes are scaled by the window's height.
            // The x-axis is centered horizontally, so the usable area is a
            // (height x height) square pillarboxed left and right.
            UniformByHeight
        };

        sf::Vector2f position;
        sf::Vector2f size;
        ScaleMode mode = ScaleMode::Stretch;

        sf::Vector2f toAbsolutePosition(const sf::Vector2u& windowSize) const;
        sf::Vector2f toAbsoluteSize(const sf::Vector2u& windowSize) const;
    };

public:

    sf::Vector2f position;
    sf::Vector2f size;

    std::optional<ScreenRelative> screen_relative;

    unsigned int mass = 1;
    float padding = 0.0f;

    UIElement() = default;
    UIElement(const sf::Vector2f& position, const sf::Vector2f& size) : position(position), size(size) {}
    UIElement(const ScreenRelative& sr) : screen_relative(sr) {}
    virtual ~UIElement() = default;

    virtual void handleEvent(const sf::Event& event) = 0;
    virtual void update(float dt) = 0;
    virtual void render(sf::RenderWindow& window) = 0;

    virtual void updateToBounds(sf::FloatRect bounds);

    [[deprecated]] void setPosition(sf::Vector2f pos);

    [[deprecated]] sf::Vector2f getPosition() const;

    [[deprecated]] void setSize(sf::Vector2f s);

    [[deprecated]] sf::Vector2f getSize() const;

    void updateScreenRelative(const sf::Vector2u& windowSize);
};

#endif // UI_ELEMENT_HPP
