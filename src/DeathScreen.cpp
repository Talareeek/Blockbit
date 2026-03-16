#include "../include/DeathScreenState.hpp"

DeathScreenState::DeathScreenState(Game* game) : GameState(game)
{
    respawn = Button{
        sf::Vector2f(300, 200),
        sf::Vector2f(200, 50),
        sf::Color::Red,
        "Respawn",
        [this]() {
            // Logic to respawn the player
            // For example, you might want to reset the player's position and health
            // and then switch back to the main game state.
            // game->respawnPlayer();
            // game->changeState(new MainGameState(game));
        }
    };
}

void DeathScreenState::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::Resized>())
    {

    }

    respawn.handleEvent(event);
}

void DeathScreenState::update(float dt)
{
    respawn.update(dt);    
}

void DeathScreenState::render(sf::RenderWindow& window)
{
    respawn.render(window);
}