#ifndef HOTBAR_HPP
#define HOTBAR_HPP

#include "UIElement.hpp"
#include "InventoryComponent.hpp"
#include "Slot.hpp"

#include <cstdint>


class Hotbar : public UIElement
{
private:

    InventoryComponent* inventory = nullptr;
    uint8_t selectedSlot = 0;

public:

    Hotbar();

    Hotbar(InventoryComponent* inventory);

    void handleEvent(const sf::Event& event) override;

    void update(float dt) override;

    void render(sf::RenderWindow& window) override;

    uint8_t getSelectedSlot() const;
};


#endif // HOTBAR_HPP