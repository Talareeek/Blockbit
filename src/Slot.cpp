#include "../include/Slot.hpp"
#include "../include/AssetManager.hpp"
#include "../include/Render.hpp"

#include <string>
#include <algorithm>

Slot::Slot() : UIElement(sf::Vector2f(0.0f, 0.0f), sf::Vector2f(20.0f, 20.0f))
{
    
}

Slot::Slot(const sf::Vector2f& relative_position, const float size, bool show_item_info) : UIElement(UIElement::ScreenRelative{relative_position, sf::Vector2f(size, size), UIElement::ScreenRelative::ScaleMode::UniformByWidth}), show_item_info{show_item_info}
{
    
}

void Slot::setItemStack(const ItemStack& stack)
{
    item_stack = stack;
}

void Slot::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::MouseMoved>())
    {
        sf::FloatRect bounds = {position, size};
        hovered = bounds.contains(static_cast<sf::Vector2f>(event.getIf<sf::Event::MouseMoved>()->position));

        mouse_pos = static_cast<sf::Vector2f>(event.getIf<sf::Event::MouseMoved>()->position);
    }
    else if(event.is<sf::Event::MouseButtonPressed>())
    {
        if(hovered)
        {
            clicked = event.getIf<sf::Event::MouseButtonPressed>()->button == sf::Mouse::Button::Left;
        }
    }
}

void Slot::update(float dt)
{
    
}

void Slot::render(sf::RenderWindow& window)
{
    sf::RectangleShape background(size * 0.9f);
    background.setPosition(position + size * 0.05f);
    sf::Color color(0, 0, 0, 128);
    background.setFillColor(background_color);
    background.setOutlineColor((hovered) ? sf::Color::White : outline_color);
    background.setOutlineThickness(size.x / 20.0f);
    window.draw(background);

    if(item_stack.empty())
    {
        return;
    }

    sf::Sprite sprite(AssetManager::getGameTexture(itemDatabase[item_stack.itemID].texture));
    sprite.setTextureRect({{0, 0}, {16, 16}});
    sprite.setPosition(position + sf::Vector2f(size.x / 10.0f, size.y / 10.0f));
    sprite.setScale({size.x * 0.8f / sprite.getLocalBounds().size.x, size.y * 0.8f / sprite.getLocalBounds().size.y});
    window.draw(sprite);


    sf::Text text(AssetManager::getFont(AssetManager::FontID::PressStart2P), std::to_string(item_stack.quantity), static_cast<unsigned int>(size.x / 3.0f));

    text.setPosition(position + sf::Vector2f(size.x / 8.0f, size.y / 8.0f));

    text.setFillColor(sf::Color::White);

    window.draw(text);

    if(hovered && show_item_info)
    {
        renderItemInfo(window.mapPixelToCoords(sf::Mouse::getPosition(window)), item_stack, window);
    }
}


void Slot::setHovered(bool hovered)
{
    this->hovered = hovered;
}

bool Slot::getHovered() const
{
    return hovered;
}