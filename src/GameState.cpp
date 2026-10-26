#include "../include/GameState.hpp"
#include "../include/Game.hpp"

GameState::GameState(Game* game) : on_top{true}, game(game)
{

}

bool GameState::alwaysUpdated() const
{
    return false;
}

bool GameState::onTop() const
{
    return on_top;
}

void GameState::onObscured()
{
    on_top = false;
}

void GameState::onRevealed()
{
    on_top = true;
}

bool GameState::renderWhenObscured()
{
    return true;
}