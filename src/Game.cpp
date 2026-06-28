#include <thread>
#include <chrono>
#include <map>
#include <iostream>

#include "../include/Game.hpp"
#include "../include/Entity.hpp"
#include "../include/PhysicsSystem.hpp"
#include "../include/RenderSystem.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/AssetManager.hpp"
#include "../include/World.hpp"
#include "../include/BlockAtlas.hpp"
#include "../include/AuthorGameState.hpp"

Game::Game()
{
    window.create(sf::VideoMode::getDesktopMode(), "Blockbit");
	
	if (!icon.loadFromFile("resources/textures/grass.png")) throw std::runtime_error("Failed loading an icon");
	
	window.setIcon(icon);

    unit_size = window.getSize().y / 9;

    AssetManager::loadTexture(200135, "resources/textures/loading_screen.png");

    sf::Sprite background(AssetManager::getTexture(200135));
    background.setPosition({0.0f, 0.0f});

    background.setScale
    (
        {
            static_cast<float>(window.getSize().x) / background.getTexture().getSize().x,
            static_cast<float>(window.getSize().y) / background.getTexture().getSize().y
        }
    );

    window.clear();

    window.draw(background);

    AssetManager::loadFont(0, "resources/fonts/PressStart2P-Regular.ttf");
    AssetManager::loadFont(1, "resources/fonts/FiraCode-Regular.ttf");

    AssetManager::loadTexture(0, "resources/textures/player.png");
    AssetManager::loadTexture(1, "resources/textures/stone.png");
    AssetManager::loadTexture(2, "resources/textures/grass.png");
    AssetManager::loadTexture(3, "resources/textures/dirt.png");
    AssetManager::loadTexture(4, "resources/textures/cobblestone.png");
    AssetManager::loadTexture(5, "resources/textures/obsidian.png");
    AssetManager::loadTexture(6, "resources/textures/bedrock.png");
    AssetManager::loadTexture(7, "resources/textures/coin.png");
    AssetManager::loadTexture(8, "resources/textures/overlay.png");
    AssetManager::loadTexture(9, "resources/textures/inventory.png");
    AssetManager::loadTexture(10, "resources/textures/hotbar.png");
    AssetManager::loadTexture(11, "resources/textures/water.png");
    AssetManager::loadTexture(12, "resources/textures/dynamite.png");
    AssetManager::loadTexture(13, "resources/textures/iron_ore.png");
    AssetManager::loadTexture(14, "resources/textures/gold_ore.png");
    AssetManager::loadTexture(15, "resources/textures/diamond_ore.png");
    AssetManager::loadTexture(16, "resources/textures/oak_log.png");
    AssetManager::loadTexture(17, "resources/textures/oak_leaves.png");
    AssetManager::loadTexture(18, "resources/textures/ui_background.png");
    AssetManager::loadTexture(19, "resources/textures/bucket.png");
    AssetManager::loadTexture(20, "resources/textures/water_bucket.png");
    AssetManager::loadTexture(21, "resources/textures/pig.png");
    AssetManager::loadTexture(22, "resources/textures/woodcutter.png");
    AssetManager::loadTexture(23, "resources/textures/logo.png");
    AssetManager::loadTexture(24, "resources/textures/lighter.png");
    AssetManager::loadTexture(25, "resources/textures/fire.png");
    AssetManager::loadTexture(UINT32_MAX, "resources/textures/nothing.png");

    AssetManager::loadSound(AssetManager::SoundID::Click, "resources/sounds/click.wav");

    AssetManager::loadMusic(AssetManager::MusicID::Alpha, "resources/sounds/music/alpha.wav");

    BlockAtlas::add(1);
    BlockAtlas::add(2);
    BlockAtlas::add(3);
    BlockAtlas::add(4);
    BlockAtlas::add(5);
    BlockAtlas::add(6);
    BlockAtlas::add(11);
    BlockAtlas::add(13);
    BlockAtlas::add(14);
    BlockAtlas::add(15);
    BlockAtlas::add(16);
    BlockAtlas::add(17);
    BlockAtlas::add(22);
    BlockAtlas::add(25);
    BlockAtlas::build();

    std::filesystem::path savePath;
    
    #ifdef _WIN32
        const char* appdata = std::getenv("APPDATA");
        if (appdata) {
            savePath = appdata;
        } else {
            savePath = std::filesystem::temp_directory_path();
        }
    #else
        const char* home = std::getenv("HOME");
        savePath = (home ? home : std::filesystem::temp_directory_path());
        savePath /= ".local/share";
    #endif
    
    savePath /= "Blockbit";
    savePath /= "account";
    
    std::filesystem::create_directories(savePath.parent_path());
    account = new Account(savePath);

    console.assignGame(this);

}

Entity& Game::entityWithID(uint32_t id)
{
    for(auto& a : entities)
    {
        if(id == a.getID()) return a;
    }
    throw std::runtime_error("Entity with ID " + std::to_string(id) + " does not exist(Game::entityWithID(int))");
}
void Game::handleEvents()
{
    while(auto event = window.pollEvent())
    {
        try
        {
            if(event->is<sf::Event::Closed>())
            {
                window.close();
            }

            else if(event->is<sf::Event::Resized>())
            {
                sf::View view(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(event->getIf<sf::Event::Resized>()->size.x), static_cast<float>(event->getIf<sf::Event::Resized>()->size.y)}));
                window.setView(view);
                window.setIcon(icon);
            }

            else if (event->is<sf::Event::KeyPressed>())
            {
                if (event->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::F11)
                {
                    fullscreen = !fullscreen;

                    window.close();

                    window.create(
                        fullscreen ? sf::VideoMode::getDesktopMode()
                                : sf::VideoMode({1280, 720}),
                        "Blockbit",
                        fullscreen ? sf::State::Fullscreen
                                : sf::State::Windowed
                    );

                    sf::View view(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)}));
                    window.setView(view);
                }
            }

            if(!gameStates.empty())
            {
                gameStates.back()->handleEvent(*event);
            }

            console.handleEvent(*event);
        }
        catch (const std::bad_alloc&)
        {
            std::cerr << "[Game] bad_alloc handling event\n";
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Game] exception handling event: " << e.what() << '\n';
        }
    }
}

void Game::update()
{
    if(!gameStates.empty())
    {
        gameStates.back()->update(dt);
    }

    console.update(dt);
}

void Game::render()
{
    window.clear();

    if(!gameStates.empty())
    {
        for(auto& state : gameStates)
        {
            state->render(window);
        }
    }
    else
    {
        exit(0);
    }

    console.render(window);

    window.display();
}

void Game::run()
{
    while(window.isOpen())
    {
        dt = clock.restart().asSeconds();

        handleEvents();
        update();
        render();
    }
}

void Game::pushState(std::unique_ptr<GameState> state)
{
    gameStates.push_back(std::move(state));
}

void Game::popState()
{
    if(!gameStates.empty())
    {
        gameStates.pop_back();
    }
}

GameState& Game::currentState()
{
    if(gameStates.empty())
    {
        throw std::runtime_error("No game states in stack");
    }
    return *gameStates.back();
}

void Game::popStates(size_t amount)
{
    for(size_t i = 0; i < amount; i++)
    {
        popState();
    }
}

Account* Game::getAccount() const
{
    return account;
}

sf::RenderWindow& Game::getWindow()
{
    return window;
}

Console& Game::getConsole()
{
    return console;
}