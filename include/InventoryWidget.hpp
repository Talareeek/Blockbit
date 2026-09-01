#ifndef INVENTORY_WIDGET_HPP
#define INVENTORY_WIDGET_HPP

#include "UIElement.hpp"
#include "InventoryComponent.hpp"
#include "Slot.hpp"
#include "Item.hpp"
#include "Button.hpp"

#include <iostream>

class InventoryWidget : public UIElement
{
private:

    InventoryComponent* inventoryComponent;
    bool active = false;

    Slot slots[36] = {};

    uint8_t page = 0;

    std::vector<Slot> crafting_slots;

    uint8_t crafting_scroll_value = 0;

    ItemStack selected_stack;

    bool craft_clicked = false;

    Button crafting_button;

    
    void renderPages(sf::RenderWindow& window);

public:

    InventoryWidget(InventoryComponent* inventoryComponent);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    bool isActive() const
    {
        return active;
    }

    void setActive(bool a)
    {
        active = a;
    }

    std::function<void(const ItemStack)> on_craft = [](const ItemStack stack){return;};
};

#endif // INVENTORY_WIDGET_HPP