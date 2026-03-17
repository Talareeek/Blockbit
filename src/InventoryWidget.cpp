#include "../include/InventoryWidget.hpp"
#include "../include/AssetManager.hpp"
#include "../include/InputManager.hpp"

#include <iostream>
#include <cmath>

InventoryWidget::InventoryWidget(InventoryComponent* inventoryComponent) : UIElement({0.0f, 0.0f}, {0.0f, 0.0f})
{
    this->inventoryComponent = inventoryComponent;
}

void InventoryWidget::handleEvent(const sf::Event& event)
{
    /*
    if(event.is<sf::Event::KeyPressed>())
    {
        auto key = event.getIf<sf::Event::KeyPressed>();

        if(key->code == sf::Keyboard::Key::E)
        {
            visible = !visible;
        }
    }
    */
}

void InventoryWidget::update(float dt)
{

}

void InventoryWidget::render(sf::RenderWindow& window)
{
    if(!active || !inventoryComponent) return;

    window.setView(window.getDefaultView());

    size = {window.getSize().x * 0.75f, window.getSize().y * 0.75f};
    position = {window.getSize().x * 0.125f, window.getSize().y * 0.125f};

    auto& texture = AssetManager::getTexture(9);

    sf::RectangleShape background({
        size.x,
        texture.getSize().y * (size.x / texture.getSize().x)
    });

    background.setPosition(position);
    background.setTexture(&texture);

    window.draw(background);

    auto& component = *inventoryComponent;

    float texWidth = texture.getSize().x;

    float borderPx = size.x * (3.0f / texWidth);
    float gapPx    = size.x * (2.0f / texWidth);
    float slotSize = size.x * (20.0f / texWidth);

    float itemSize = slotSize * (16.0f / 20.0f);

    for(size_t i = 0; i < component.inventory.slots.size(); i++)
    {
        size_t col = i % 9;
        size_t row = i / 9;

        auto& item = component.inventory.slots[i];
        if(item.itemID == ItemID::None) continue;

        sf::Vector2f slotPosition = position + sf::Vector2f(
            borderPx + col * (slotSize + gapPx),
            borderPx + row * (slotSize + gapPx)
        );

        sf::Vector2f itemPosition = slotPosition + sf::Vector2f(
            (slotSize - itemSize) / 2.0f,
            (slotSize - itemSize) / 2.0f
        );

        sf::Sprite sprite(AssetManager::getTexture(itemDatabase[item.itemID].texture));

        sprite.setPosition({
            std::round(itemPosition.x),
            std::round(itemPosition.y)
        });

        sprite.setScale({
            itemSize / sprite.getTextureRect().size.x,
            itemSize / sprite.getTextureRect().size.y
        });

        window.draw(sprite);
    }
}