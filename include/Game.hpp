#ifndef GAME_HPP
#define GAME_HPP

#include <SFML/Graphics.hpp>
#include <vector>
#include "Entity.hpp"
#include "World.hpp"
#include "GameState.hpp"
#include "Account.hpp"
#include "Console.hpp"

class Game
{
private:

    sf::RenderWindow window;

    unsigned int unit_size;

    uint8_t blue = 254;
    uint8_t green = 127;

    bool fullscreen = false;

    Account* account;

    std::vector<Entity> entities;

    Entity& entityWithID(uint32_t id);

    void handleEvents();

    void update();

    void render();

    std::vector<std::unique_ptr<GameState>> gameStates;

    sf::Clock clock;

    float dt;

    Console console{{100.0f, 100.0f}, {250.0f, 200.0f}};


public:

    Game();

    void run();

    // GAME STATES
    void pushState(std::unique_ptr<GameState> state);
    void popState();
    void popStates(size_t amount);
    GameState& currentState();

    //ACCOUNT
    Account* getAccount() const;

    sf::RenderWindow& getWindow();

    Console& getConsole();


};

#endif // GAME_HPP