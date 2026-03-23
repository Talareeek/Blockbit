#ifndef MAIN_GAME_STATE_HPP
#define MAIN_GAME_STATE_HPP

#include "GameState.hpp"
#include "World.hpp"
#include "HealthBar.hpp"
#include "InventoryWidget.hpp"
#include "Hotbar.hpp"
#include "Button.hpp"

class MainGameState : public GameState
{
private:

    World world;
    HealthBar healthBar;

    InventoryWidget inventoryWidget{nullptr};
    Hotbar hotbar;

public:

    MainGameState(Game* game);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

};

#endif // MAIN_GAME_STATE_HPP