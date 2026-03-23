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


sf::Vector2f computeAbsolutePosition(const UIElement::ScreenRelative& sr, const sf::Vector2u& windowSize)
{
    if(sr.keep_aspect)
    {
        float base;
        float offsetX = 0.f;
        float offsetY = 0.f;

        if(sr.axis == UIElement::ScreenRelative::Axis::Y)
        {
            base = static_cast<float>(windowSize.y);
            offsetX = (windowSize.x - base) / 2.f;
        }
        else
        {
            base = static_cast<float>(windowSize.x);
            offsetY = (windowSize.y - base) / 2.f;
        }

        return {
            sr.position.x * base + offsetX,
            sr.position.y * base + offsetY
        };
    }
    else
    {
        return {
            sr.position.x * windowSize.x,
            sr.position.y * windowSize.y
        };
    }
}


sf::Vector2f computeAbsoluteSize(const UIElement::ScreenRelative& sr, const sf::Vector2u& windowSize)
{
    sf::Vector2f size;

    if(sr.keep_aspect)
    {
        if(sr.axis == UIElement::ScreenRelative::Axis::X)
        {
            float base = sr.size.x * windowSize.x;
            float ratio = sr.size.y / sr.size.x;

            size.x = base;
            size.y = base * ratio;
        }
        else
        {
            float base = sr.size.y * windowSize.y;
            float ratio = sr.size.x / sr.size.y;

            size.y = base;
            size.x = base * ratio;
        }
    }
    else
    {
        size = {
            sr.size.x * windowSize.x,
            sr.size.y * windowSize.y
        };
    }

    return size;
}


void UIElement::updateScreenRelative(const sf::Vector2u& windowSize)
{
    if(is_screen_relative)
    {
        size = computeAbsoluteSize(screen_relative, windowSize);
        position = computeAbsolutePosition(screen_relative, windowSize);       
    }
}