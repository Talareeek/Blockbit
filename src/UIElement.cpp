#include "../include/UIElement.hpp"

void UIElement::setPosition(sf::Vector2f pos)
{
    position = pos;
}

sf::Vector2f UIElement::getPosition() const
{
    return position;
}

void UIElement::setSize(sf::Vector2f s)
{
    size = s;
}

sf::Vector2f UIElement::getSize() const
{
    return size;
}

sf::Vector2f UIElement::ScreenRelative::toAbsolutePosition(const sf::Vector2u& windowSize) const
{
    switch(mode)
    {
        case ScaleMode::Stretch:
            return {
                position.x * windowSize.x,
                position.y * windowSize.y
            };

        case ScaleMode::UniformByWidth:
        {
            float base = static_cast<float>(windowSize.x);
            float offsetY = (static_cast<float>(windowSize.y) - base) / 2.f;
            return {
                position.x * base,
                position.y * base + offsetY
            };
        }

        case ScaleMode::UniformByHeight:
        {
            float base = static_cast<float>(windowSize.y);
            float offsetX = (static_cast<float>(windowSize.x) - base) / 2.f;
            return {
                position.x * base + offsetX,
                position.y * base
            };
        }
    }

    return {};
}

sf::Vector2f UIElement::ScreenRelative::toAbsoluteSize(const sf::Vector2u& windowSize) const
{
    switch(mode)
    {
        case ScaleMode::Stretch:
            return {
                size.x * windowSize.x,
                size.y * windowSize.y
            };

        case ScaleMode::UniformByWidth:
            return {
                size.x * windowSize.x,
                size.y * windowSize.x
            };

        case ScaleMode::UniformByHeight:
            return {
                size.x * windowSize.y,
                size.y * windowSize.y
            };
    }

    return {};
}

void UIElement::updateScreenRelative(const sf::Vector2u& windowSize)
{
    if(screen_relative)
    {
        size = screen_relative->toAbsoluteSize(windowSize);
        position = screen_relative->toAbsolutePosition(windowSize);
    }
}

void UIElement::updateToBounds(sf::FloatRect bounds)
{
    size = {bounds.size.x - 2 * padding, bounds.size.y - 2 * padding};
    position = bounds.position + sf::Vector2f{padding, padding};
}