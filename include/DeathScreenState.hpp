#ifndef DEATH_SCREEN_STATE_HPP
#define DEATH_SCREEN_STATE_HPP

#include "GameState.hpp"
#include "Button.hpp"

class DeathScreenState : public GameState
{
private:

    Button respawn;

public:

    DeathScreenState(Game* game);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

};

#endif // DEATH_SCREEN_STATE_HPP