#include "../include/Hotbar.hpp"
#include "../include/AssetManager.hpp"
#include <iostream>


Hotbar::Hotbar() : UIElement(UIElement::ScreenRelative{{0.2f, 0.8f}, {0.6f, 0.6f * (13.0f / 101.0f)}, true, UIElement::ScreenRelative::Axis::Y})
{

}

Hotbar::Hotbar(InventoryComponent* inventory) : UIElement(UIElement::ScreenRelative{{0.2f, 0.8f}, {0.6f, 0.6f * (13.0f / 101.0f)}, true, UIElement::ScreenRelative::Axis::Y}), inventory(inventory)
{

}

void Hotbar::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::MouseWheelScrolled>())
    {
        auto mouse = event.getIf<sf::Event::MouseWheelScrolled>();

        int delta = -(static_cast<int>(mouse->delta));

        selectedSlot = (selectedSlot + delta + 9) % 9;
    }
}

void Hotbar::update(float dt)
{

}


void Hotbar::render(sf::RenderWindow& window)
{
    sf::RectangleShape background(size);
    background.setPosition(position);
    background.setTexture(&AssetManager::getTexture(10));
    window.draw(background);

    float slotWidth = size.x * (10.f / 101.f);
    float slotSpacing = size.x * (2.f / 202.f);
    float slotStart = size.x * (3.f / 202.f);
    float verticalOffset = size.y * (3.f / 26.f);

    sf::RectangleShape selector({slotWidth, slotWidth});
    selector.setFillColor(sf::Color::Transparent);
    selector.setOutlineColor(sf::Color::White);
    selector.setOutlineThickness(2.f);

    selector.setPosition(
        position + sf::Vector2f(
            slotStart + (slotWidth + slotSpacing) * selectedSlot,
            verticalOffset
        )
    );

    window.draw(selector);

    if(!inventory) return;

    float itemSize = slotWidth - slotSpacing;

    for(int i = 0; i < 9; i++)
    {
        ItemStack& stack = inventory->inventory.slots[i];

        if(stack.itemID == static_cast<ItemID>(0))
            continue;

        sf::Sprite itemSprite(AssetManager::getTexture(itemDatabase[stack.itemID].texture));

        itemSprite.setPosition(
            position + sf::Vector2f(
                slotStart + (slotWidth + slotSpacing) * i + slotSpacing/2,
                verticalOffset + slotSpacing/2
            )
        );

        itemSprite.setScale({itemSize / 16.f, itemSize / 16.f});

        window.draw(itemSprite);
    }
}

/*void Hotbar::render(sf::RenderWindow& window)
{
    sf::RectangleShape background(size);
    background.setPosition(position);
    background.setTexture(&AssetManager::getTexture(10));

    window.draw(background);


    float slotWidth = size.x * (10.0f / 101.0f);

    sf::RectangleShape selector({slotWidth, slotWidth});
    selector.setFillColor(sf::Color::Transparent);
    selector.setOutlineColor(sf::Color::White);
    selector.setOutlineThickness(2.0f);

    selector.setPosition(
        position + sf::Vector2f(
            (3.0f / 202.0f * size.x) + (slotWidth + (2.0f / 202.0f * size.x)) * selectedSlot,
            (3.0f / 26.0f) * size.y
        )
    );

    window.draw(selector);

    if(!inventory) return;

    for(int i = 0; i < 9; i++)
    {        
        ItemStack& stack = inventory->inventory.slots[i];

        if(stack.itemID != static_cast<ItemID>(0))
        {
            sf::Sprite itemSprite(AssetManager::getTexture(itemDatabase[stack.itemID].texture));

            itemSprite.setPosition(
                position + sf::Vector2f(
                    (3.0f / 202.0f * size.x) + (slotWidth + (5.0f / 202.0f * size.x)) * i,
                    (5.0f / 26.0f) * size.y
                )
            );

            itemSprite.setScale({(slotWidth - (2.0f / 202.0f * size.x)) / 16.0f, (slotWidth - (2.0f / 202.0f * size.x)) / 16.0f});

            window.draw(itemSprite);
        }
    }
}*/


uint8_t Hotbar::getSelectedSlot() const
{
    return selectedSlot;
}
