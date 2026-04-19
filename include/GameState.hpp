#ifndef GAME_STATE_HPP
#define GAME_STATE_HPP

#include <SFML/Graphics.hpp>

class Game;

class GameState
{
protected:

    Game* game;

public:

    GameState(Game* game);

    virtual void handleEvent(const sf::Event& event) = 0;
    virtual void update(float dt) = 0;
    virtual void render(sf::RenderWindow& window) = 0;

    virtual ~GameState() = default;
};

#endif // GAME_STATE_HPP