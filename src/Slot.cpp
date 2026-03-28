#include "../include/Slot.hpp"
#include "../include/AssetManager.hpp"

#include <string>

Slot::Slot() : UIElement(sf::Vector2f(0.0f, 0.0f), sf::Vector2f(20.0f, 20.0f))
{
    
}

Slot::Slot(const sf::Vector2f& relative_position, const float size) : UIElement(UIElement::ScreenRelative{relative_position, sf::Vector2f(size, size), true, UIElement::ScreenRelative::Axis::X})
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
    }
}

void Slot::update(float dt)
{
    
}

void Slot::render(sf::RenderWindow& window)
{
    sf::RectangleShape background(size * 0.9f);
    background.setPosition(position + size * 0.05f);
    sf::Color color(255, 255, 255, 128);
    background.setFillColor(color);
    background.setOutlineColor((hovered) ? sf::Color::White : sf::Color::Black);
    background.setOutlineThickness(size.x / 20.0f);
    window.draw(background);


    sf::Sprite sprite(AssetManager::getTexture(itemDatabase[item_stack.itemID].texture));
    sprite.setPosition(position + sf::Vector2f(size.x / 10.0f, size.y / 10.0f));
    sprite.setScale({size.x * 0.8f / sprite.getTexture().getSize().x, size.y * 0.8f / sprite.getTexture().getSize().y});
    window.draw(sprite);


    sf::Text text(AssetManager::getFont(0), std::to_string(item_stack.quantity), static_cast<unsigned int>(size.x / 3.0f));

    text.setPosition(position + sf::Vector2f(size.x / 8.0f, size.y / 8.0f));

    text.setFillColor(sf::Color::White);

    window.draw(text);

}


void Slot::setHovered(bool hovered)
{
    this->hovered = hovered;
}

bool Slot::getHovered() const
{
    return hovered;
}