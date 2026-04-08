#ifndef INTRO_GAME_STATE_HPP
#define INTRO_GAME_STATE_HPP

#include "GameState.hpp"

class IntroGameState : public GameState
{
private:

    static constexpr float animationTime = 3.0f;
    float timer = 0.0f;

public:

    using GameState::GameState;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

};

#endif // INTRO_GAME_STATE_HPP