#include <SFML/Graphics.hpp>

#include "../include/Game.hpp"

#include "../include/AssetManager.hpp"

#include "../include/MainGameState.hpp"

#include "../include/MenuGameState.hpp"

#include <cstdlib>

#include <thread>

int main()
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    Game game;

    game.pushState(std::make_unique<MenuGameState>(&game));

    game.run();
}