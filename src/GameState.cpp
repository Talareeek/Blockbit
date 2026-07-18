#include "../include/GameState.hpp"
#include "../include/Game.hpp"

GameState::GameState(Game* game) : game(game)
{

}

bool GameState::alwaysUpdated() const
{
    return false;
}