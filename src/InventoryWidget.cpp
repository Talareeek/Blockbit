#include "../include/InventoryWidget.hpp"
#include "../include/AssetManager.hpp"

InventoryWidget::InventoryWidget(InventoryComponent* inventoryComponent) : UIElement({0.0f, 0.0f}, {0.0f, 0.0f})
{
    this->inventoryComponent = inventoryComponent;
}

void InventoryWidget::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::KeyPressed>())
    {
        auto key = event.getIf<sf::Event::KeyPressed>();

        if(key->code == sf::Keyboard::Key::E)
        {
            visible = !visible;
        }
    }
}

void InventoryWidget::update(float dt)
{

}

void InventoryWidget::render(sf::RenderWindow& window)
{

    //NEW POSITION AND SIZE
    size = {window.getSize().x * 0.75f, window.getSize().y * 0.75f};
    position = {window.getSize().x * 0.125f, window.getSize().y * 0.125f};

    if(!visible) return;

    window.setView(window.getDefaultView());

    sf::RectangleShape background(size);
    background.setPosition(position);

    background.setTexture(&(AssetManager::getTexture(9)));

    window.draw(background);

    if(!inventoryComponent) return;
    auto& component = *inventoryComponent;


    for(size_t i = 0; i < component.inventory.slots.size(); i++)
    {
        size_t col = i % 9;
        size_t row = i / 9;

        auto& item = component.inventory.slots[i];

        sf::Vector2f itemPosition = position + sf::Vector2f(3.0f / size.x + col * (20.0f / size.x) + (2.0f / size.x), 3.0f / size.y + row * (20.0f / size.y) + (2.0f / size.y));
        sf::Vector2f itemSize = {20.0f / size.x, 20.0f / size.y};

        sf::Sprite sprite(AssetManager::getTexture(itemDatabase[item.itemID].texture));
        sprite.setPosition(itemPosition);
        sprite.setScale(itemSize);

        window.draw(sprite);
    }
}