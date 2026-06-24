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

#include "../include/IntroGameState.hpp"

#include "../include/ClientGameState.hpp"

#include <cstdlib>

#include <thread>

#include <string>

#include <filesystem>

#include <iostream>

int main(int argc, char* argv[])
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    std::string loadWorld;
    std::string joinAddress;
    std::string hostWorld;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--load" && i + 1 < argc)
        {
            loadWorld = argv[++i];
        }
        else if (arg == "--join" && i + 1 < argc)
        {
            joinAddress = argv[++i];
        }
        else if (arg == "--host" && i + 1 < argc)
        {
            hostWorld = argv[++i];
        }
    }

    Game game;

    if (!joinAddress.empty())
    {
        std::string host = joinAddress;
        uint16_t port = 25565;

        auto colon = joinAddress.find(':');
        if (colon != std::string::npos)
        {
            host = joinAddress.substr(0, colon);
            std::string port_str = joinAddress.substr(colon + 1);
            try { port = static_cast<uint16_t>(std::stoi(port_str)); }
            catch (...) { port = 25565; }
        }

        std::cerr << "[main] --join " << host << ":" << port << std::endl;
        game.pushState(std::make_unique<ClientGameState>(&game, host, port));
    }
    else if (!hostWorld.empty())
    {
        std::filesystem::path worldPath = std::filesystem::path(std::getenv("HOME")) / "Blockbit" / "saves" / hostWorld;
        if (std::filesystem::exists(worldPath))
        {
            std::cerr << "[main] --host " << hostWorld << " on 25565" << std::endl;
            game.pushState(std::make_unique<ClientGameState>(&game, World(worldPath), ClientGameState::DEFAULT_PORT));
        }
        else
        {
            std::cerr << "World not found at: " << worldPath << std::endl;
            game.pushState(std::make_unique<IntroGameState>(&game));
        }
    }
    else if (!loadWorld.empty())
    {
        std::filesystem::path worldPath = std::filesystem::path(std::getenv("HOME")) / "Blockbit" / "saves" / loadWorld;
        if (std::filesystem::exists(worldPath))
        {
            try
            {
                game.pushState(std::make_unique<ClientGameState>(&game, World(worldPath)));
            }
            catch (const std::exception& e)
            {
                std::cerr << "Failed to load world: " << e.what() << std::endl;
                game.pushState(std::make_unique<IntroGameState>(&game));
            }
        }
        else
        {
            std::cerr << "World not found at: " << worldPath << std::endl;
            game.pushState(std::make_unique<IntroGameState>(&game));
        }
    }
    else
    {
        game.pushState(std::make_unique<IntroGameState>(&game));
    }

    game.run();

    return 0;
}