#ifndef SLOT_HPP
#define SLOT_HPP

#include "UIElement.hpp"
#include "Item.hpp"

class Slot : public UIElement
{

private:

    ItemStack item_stack;

    bool hovered = false;

public:

    using UIElement::UIElement;

    Slot();
    Slot(const sf::Vector2f& relative_position, const float size);

    void setItemStack(const ItemStack& stack);

    void setHovered(bool hovered);
    bool getHovered() const;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

};

#endif // SLOT_HPP