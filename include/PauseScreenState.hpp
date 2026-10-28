#ifndef PAUSE_SCREEN_HPP
#define PAUSE_SCREEN_HPP

#include "GameState.hpp"
#include "TextButton.hpp"

#include <SFML/Graphics/Texture.hpp>

class ClientGameState;

class PauseScreenState : public GameState
{
private:

    ClientGameState* client_state = nullptr;

    sf::Texture blockbit_logo;

    TextButton resume;
    TextButton settings;
    TextButton save_world;
    TextButton leave;

    bool has_local_server = false;
    bool save_feedback = false;
    float save_feedback_timer = 0.0f;

public:

    PauseScreenState(Game* game, ClientGameState* client_state);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // PAUSE_SCREEN_HPP
