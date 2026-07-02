#ifndef MENU_GAME_STATE_HPP
#define MENU_GAME_STATE_HPP

#include "GameState.hpp"
#include "Button.hpp"
#include "AccountWidget.hpp"
#include "Slot.hpp"
#include "WorldList.hpp"
#include "LobbyPlayerElement.hpp"
#include "Slider.hpp"

#include "Hotbar.hpp"

class MenuGameState : public GameState
{
private:

    Button quit;
    WorldList worldList;
    LobbyPlayerElement player;

public:

    MenuGameState(Game* game);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // MENU_GAME_STATE_HPP