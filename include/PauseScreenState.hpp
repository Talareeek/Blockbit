#ifndef PAUSE_SCREEN_HPP
#define PAUSE_SCREEN_HPP

#include "GameState.hpp"
#include "Button.hpp"

class PauseScreenState : public GameState
{
private:

    Button resume;
    Button quit;

public:

    PauseScreenState(Game* game);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // PAUSE_SCREEN_HPP