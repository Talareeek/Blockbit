#ifndef AUTHOR_GAME_STATE
#define AUTHOR_GAME_STATE

#include "GameState.hpp"

class AuthorGameState : public GameState
{
private:

    float length = 0.0f;

public:

    using GameState::GameState;

    const float ANIMATION_LENGTH = 3.0f;

    void update(float dt) override;
    void handleEvent(const sf::Event& event) override;
    void render(sf::RenderWindow& window) override;

};

#endif // AUTHOR_GAME_STATE