#include "../include/Slot.hpp"
#include "../include/AssetManager.hpp"

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
}

void Slot::update(float dt)
{
    
}

void Slot::render(sf::RenderWindow& window)
{
    sf::RectangleShape background(size * 0.9f);
    background.setPosition(position + size * 0.05f);
    sf::Color color(0, 0, 0, 128);
    background.setFillColor(color);
    background.setOutlineColor((hovered) ? sf::Color::White : sf::Color::Black);
    background.setOutlineThickness(size.x / 20.0f);
    window.draw(background);

    if(item_stack.empty())
    {
        return;
    }

    sf::Sprite sprite(AssetManager::getTexture(itemDatabase[item_stack.itemID].texture));
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
        const auto& itemData = itemDatabase[item_stack.itemID];

        std::string nameStr = itemData.name;
        std::string idStr = "ID: " + std::to_string(static_cast<uint32_t>(item_stack.itemID));

        unsigned int charSize = static_cast<unsigned int>(size.x / 5.0f);

        sf::Text nameText(AssetManager::getFont(AssetManager::FontID::PressStart2P), nameStr, charSize);
        sf::Text idText(AssetManager::getFont(AssetManager::FontID::PressStart2P), idStr, charSize);

        sf::Color rarityColor;

        switch(itemDatabase[item_stack.itemID].rarity)
        {
        case ItemRarity::Common:

            rarityColor = sf::Color::White;
            break;

        case ItemRarity::Rare:

            rarityColor = sf::Color::Green;
            break;

        case ItemRarity::Super_Rare:

            rarityColor = sf::Color::Blue;
            break;

        case ItemRarity::Epic:

            rarityColor = sf::Color(128, 0, 128);
            break;

        case ItemRarity::Mythic:

            rarityColor = sf::Color::Red;
            break;
        }

        nameText.setFillColor(rarityColor);
        idText.setFillColor(sf::Color::White);

        float padding = static_cast<float>(charSize) * 0.4f;
        float lineSpacing = static_cast<float>(charSize) * 0.3f;

        sf::FloatRect nameBounds = nameText.getLocalBounds();
        sf::FloatRect idBounds = idText.getLocalBounds();

        float frameWidth = std::max(nameBounds.size.x, idBounds.size.x) + padding * 2.0f;
        float frameHeight = nameBounds.size.y + idBounds.size.y + lineSpacing + padding * 2.0f;

        sf::Vector2f framePos = mouse_pos + sf::Vector2f(15.0f, 15.0f);

        sf::Vector2u windowSize = window.getSize();
        if(framePos.x + frameWidth > windowSize.x)
        {
            framePos.x = windowSize.x - frameWidth;
        }
        if(framePos.y + frameHeight > windowSize.y)
        {
            framePos.y = windowSize.y - frameHeight;
        }

        sf::RectangleShape frame({frameWidth, frameHeight});
        frame.setPosition(framePos);
        frame.setFillColor(sf::Color(0, 0, 0, 200));
        frame.setOutlineColor(sf::Color::White);
        frame.setOutlineThickness(1.0f);
        window.draw(frame);

        nameText.setPosition(framePos + sf::Vector2f(padding, padding));
        idText.setPosition(framePos + sf::Vector2f(padding, padding + nameBounds.size.y + lineSpacing));

        window.draw(nameText);
        window.draw(idText);
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