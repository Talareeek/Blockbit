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

#include <string>

#include <filesystem>

int main(int argc, char* argv[])
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    Game game;

    // Check for --load argument
    std::string loadWorld;
    if(argc >= 3 && std::string(argv[1]) == "--load") {
        loadWorld = argv[2];
    }

    if(!loadWorld.empty()) {
        // Load specific world
        std::filesystem::path worldPath = std::filesystem::path(std::getenv("HOME")) / "Blockbit" / "saves" / loadWorld;
        if(std::filesystem::exists(worldPath)) {
            try {
                game.pushState(std::make_unique<MainGameState>(&game, World(worldPath)));
            } catch(const std::exception& e) {
                std::cerr << "Failed to load world: " << e.what() << std::endl;
                game.pushState(std::make_unique<IntroGameState>(&game));
            }
        } else {
            std::cerr << "World not found at: " << worldPath << std::endl;
            game.pushState(std::make_unique<IntroGameState>(&game));
        }
    } else {
        // Default: show intro
        game.pushState(std::make_unique<IntroGameState>(&game));
    }

    game.run();
}