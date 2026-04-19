/**
 *     ______   _        _______  _______  _        ______  __________________
 *    (  ___ \ ( \      (  ___  )(  ____ \| \    /\(  ___ \ \__   __/\__   __/
 *    | (   ) )| (      | (   ) || (    \/|  \  / /| (   ) )   ) (      ) (   
 *    | (__/ / | |      | |   | || |      |  (_/ / | (__/ /    | |      | |   
 *    |  __ (  | |      | |   | || |      |   _ (  |  __ (     | |      | |   
 *    | (  \ \ | |      | |   | || |      |  ( \ \ | (  \ \    | |      | |   
 *    | )___) )| (____/\| (___) || (____/\|  /  \ \| )___) )___) (___   | |   
 *    |/ \___/ (_______/(_______)(_______/|_/    \/|/ \___/ \_______/   )_(   
 *                                                                            
 */


#include <SFML/Graphics.hpp>

#include "../include/Game.hpp"

#include "../include/AssetManager.hpp"

#include "../include/MainGameState.hpp"

#include "../include/IntroGameState.hpp"

#include <cstdlib>

#include <thread>

int main()
{

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    Game game;

    game.pushState(std::make_unique<IntroGameState>(&game));

    game.run();
}