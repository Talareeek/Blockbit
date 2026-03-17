#ifndef SLOT_HPP
#define SLOT_HPP

#include "Item.hpp"
#include "UIElement.hpp"

class Slot : public UIElement
{
private:

    ItemStack item;

    bool hovered;

public:

    

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // SLOT_HPP