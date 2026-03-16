#ifndef INVENTORY_WIDGET_HPP
#define INVENTORY_WIDGET_HPP

#include "UIElement.hpp"
#include "InventoryComponent.hpp"

class InventoryWidget : public UIElement
{
private:

    InventoryComponent* inventoryComponent;
    bool visible = false;

public:

    InventoryWidget(InventoryComponent* inventoryComponent);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // INVENTORY_WIDGET_HPP