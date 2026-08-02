#ifndef DEATH_SCREEN_STATE_HPP
#define DEATH_SCREEN_STATE_HPP

#include "GameState.hpp"
#include "Button.hpp"
#include "World.hpp"

class DeathScreenState : public GameState
{
private:

    Button respawn;
    UUID entityID;
    World& world;

public:

    DeathScreenState(Game* game, World& world, UUID entityID);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

};

#endif // DEATH_SCREEN_STATE_HPP