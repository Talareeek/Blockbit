#ifndef MENU_GAME_STATE_HPP
#define MENU_GAME_STATE_HPP

#include "GameState.hpp"
#include "Button.hpp"
#include "AccountWidget.hpp"

class MenuGameState : public GameState
{
private:

    Button play;
    Button quit;
    AccountWidget accountWidget;

public:

    MenuGameState(Game* game);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // MENU_GAME_STATE_HPP