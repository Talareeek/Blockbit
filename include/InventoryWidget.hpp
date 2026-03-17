#ifndef INVENTORY_WIDGET_HPP
#define INVENTORY_WIDGET_HPP

#include "UIElement.hpp"
#include "InventoryComponent.hpp"

#include <iostream>

class InventoryWidget : public UIElement
{
private:

    InventoryComponent* inventoryComponent;
    bool active = false;

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
};

#endif // INVENTORY_WIDGET_HPP