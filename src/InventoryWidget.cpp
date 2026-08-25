#include "../include/InventoryWidget.hpp"
#include "../include/AssetManager.hpp"
#include "../include/InputManager.hpp"
#include "../include/Slot.hpp"

#include <iostream>
#include <cmath>

InventoryWidget::InventoryWidget(InventoryComponent* inventoryComponent) : UIElement({0.0f, 0.0f}, {0.0f, 0.0f})
{
    this->inventoryComponent = inventoryComponent;

    if(!inventoryComponent) return;

    for(int i = 0; i < 36; i++)
    {
        slots[i].setItemStack(inventoryComponent->inventory.slots[i]);
    }
}

void InventoryWidget::handleEvent(const sf::Event& event)
{
    for(int i = 0; i < 36; i++)
    {
        slots[i].handleEvent(event);
    }
}

void InventoryWidget::update(float dt)
{
    if(!inventoryComponent) return;

    for(int i = 0; i < 36; i++)
    {
        slots[i].setItemStack(inventoryComponent->inventory.slots[i]);
    }
}

void InventoryWidget::render(sf::RenderWindow& window)
{
    if(!active || !inventoryComponent) return;

    size = {window.getSize().x * 0.75f, window.getSize().y * 0.75f};
    position = {window.getSize().x * 0.125f, window.getSize().y * 0.125f};

    auto& texture = AssetManager::getUITexture(AssetManager::UITextureID::Inventory);

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

    for(int i = 35; i >= 0; i--)
    {
        int col = i % 9;
        int row = i / 9;

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

        slots[i].position = slotPosition;
        slots[i].size = {slotSize, slotSize};

        slots[i].render(window);
    }
}