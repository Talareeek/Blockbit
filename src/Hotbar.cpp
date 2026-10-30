#include "../include/Hotbar.hpp"
#include "../include/AssetManager.hpp"
#include <iostream>


Hotbar::Hotbar() : UIElement(UIElement::ScreenRelative{{0.2f, 0.8f}, {0.6f, 0.6f * (13.0f / 101.0f)}, UIElement::ScreenRelative::ScaleMode::UniformByHeight})
{

}

Hotbar::Hotbar(InventoryComponent* inventory) : UIElement(UIElement::ScreenRelative{{0.2f, 0.8f}, {0.6f, 0.6f * (13.0f / 101.0f)}, UIElement::ScreenRelative::ScaleMode::UniformByHeight}), inventory(inventory)
{

}

void Hotbar::handleEvent(const sf::Event& event)
{
    std::cout << "Handling\n";

    if((event.is<sf::Event::MouseButtonPressed>() && event.getIf<sf::Event::MouseButtonPressed>()->button == sf::Mouse::Button::Left) || event.is<sf::Event::TouchBegan>())
    {
        std::cout << "Clicled\n";

        sf::Vector2i position = (event.is<sf::Event::MouseButtonPressed>()) ? event.getIf<sf::Event::MouseButtonPressed>()->position : event.getIf<sf::Event::TouchBegan>()->position;

        for(int i = 0; i < 9; i++)
        {
            sf::IntRect slot_rect({static_cast<int>(slots[i].position.x), static_cast<int>(slots[i].position.y)}, {static_cast<int>(slots[i].size.x), static_cast<int>(slots[i].size.y)});

            if(slot_rect.contains(position)) setSelectedSlot(i);
        }
    }
}

void Hotbar::update(float dt)
{

}


void Hotbar::render(sf::RenderWindow& window)
{
    sf::RectangleShape background(size);
    background.setPosition(position);
    background.setTexture(&AssetManager::getUITexture(AssetManager::UITextureID::Hotbar));
    window.draw(background);

    float slot_width = size.x * (10.f / 101.f);
    float slot_spacing = size.x * (2.f / 202.f);
    float slot_start = size.x * (3.f / 202.f);
    float vertical_offset = size.y * (3.f / 26.f);

    if(!inventory) return;

    for(int i = 0; i < 9; i++)
    {
        ItemStack& stack = inventory->inventory.slots[i];

        slots[i] = Slot(position + sf::Vector2f(slot_start + (slot_width + slot_spacing) * i,vertical_offset), {slot_width, slot_width});

        slots[i].setItemStack(stack);

        slots[i].setHovered(i == inventory->selectedSlot);

        slots[i].render(window);
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
