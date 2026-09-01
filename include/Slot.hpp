#ifndef SLOT_HPP
#define SLOT_HPP

#include "UIElement.hpp"
#include "Item.hpp"

class Slot : public UIElement
{

private:

    bool hovered = false;

    sf::Vector2f mouse_pos;

public:

    ItemStack item_stack;

    using UIElement::UIElement;

    Slot();
    Slot(const sf::Vector2f& relative_position, const float size, bool show_item_info = true);

    void setItemStack(const ItemStack& stack);

    void setHovered(bool hovered);
    bool getHovered() const;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    bool show_item_info = false;

    sf::Color background_color{0, 0, 0, 128};
    sf::Color outline_color{sf::Color::Black};

    bool clicked = false;
};

#endif // SLOT_HPP