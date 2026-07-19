#include "../include/GameState.hpp"
#include "../include/Game.hpp"

GameState::GameState(Game* game) : game(game), on_top{true}
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