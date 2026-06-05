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
    if(!inventory) return;

    if(event.is<sf::Event::MouseWheelScrolled>())
    {
        auto mouse = event.getIf<sf::Event::MouseWheelScrolled>();

        int delta = -(static_cast<int>(mouse->delta));

        inventory->selectedSlot = (inventory->selectedSlot + delta + 9) % 9;
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

    if(!inventory) return;

    float itemSize = slotWidth - slotSpacing;

    for(int i = 0; i < 9; i++)
    {
        ItemStack& stack = inventory->inventory.slots[i];

        Slot slot(position + sf::Vector2f(slotStart + (slotWidth + slotSpacing) * i,verticalOffset), {slotWidth, slotWidth});

        slot.setItemStack(stack);

        slot.setHovered(i == inventory->selectedSlot);

        slot.render(window);
    }
}

uint8_t Hotbar::getSelectedSlot() const
{
    return inventory ? inventory->selectedSlot : 0;
}

void Hotbar::setSelectedSlot(uint8_t slot)
{
    if(!inventory) return;
    inventory->selectedSlot = slot % 9;
}
