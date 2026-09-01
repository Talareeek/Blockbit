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

    AssetManager::loadFont(AssetManager::FontID::PressStart2P, "resources/fonts/PressStart2P-Regular.ttf");
    AssetManager::loadFont(AssetManager::FontID::FiraCode, "resources/fonts/FiraCode-Regular.ttf");

    //UI TEXTURES
    AssetManager::loadUITexture(AssetManager::UITextureID::Coin, "resources/textures/coin.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::Block_Overlay, "resources/textures/overlay.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::Inventory, "resources/textures/inventory.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::Hotbar, "resources/textures/hotbar.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::UIBackground, "resources/textures/ui_background.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::Logo, "resources/textures/logo.png");


    //GAME TEXTURES
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Player, "resources/textures/player.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Dynamite, "resources/textures/dynamite.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Bucket, "resources/textures/bucket.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Water_Bucket, "resources/textures/water_bucket.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Pig, "resources/textures/pig.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Lighter, "resources/textures/lighter.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Wooden_Pickaxe, "resources/textures/wooden_pickaxe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Stone_Pickaxe, "resources/textures/stone_pickaxe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Gold_Pickaxe, "resources/textures/gold_pickaxe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Iron_Pickaxe, "resources/textures/iron_pickaxe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Diamond_Pickaxe, "resources/textures/diamond_pickaxe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Wooden_Axe, "resources/textures/wooden_axe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Stone_Axe, "resources/textures/stone_axe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Gold_Axe, "resources/textures/gold_axe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Iron_Axe, "resources/textures/iron_axe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Diamond_Axe, "resources/textures/diamond_axe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Wooden_Shovel, "resources/textures/wooden_shovel.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Stone_Shovel, "resources/textures/stone_shovel.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Gold_Shovel, "resources/textures/gold_shovel.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Iron_Shovel, "resources/textures/iron_shovel.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Diamond_Shovel, "resources/textures/diamond_shovel.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Ruby, "resources/textures/ruby.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Stick, "resources/textures/stick.png");

    AssetManager::loadGameTexture(AssetManager::GameTextureID::Stone, "resources/textures/stone.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Grass, "resources/textures/grass.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Dirt, "resources/textures/dirt.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Cobblestone, "resources/textures/cobblestone.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Obsidian, "resources/textures/obsidian.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Bedrock, "resources/textures/bedrock.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Water, "resources/textures/water.png");    
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Iron_Ore, "resources/textures/iron_ore.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Gold_Ore, "resources/textures/gold_ore.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Diamond_Ore, "resources/textures/diamond_ore.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Oak_Log, "resources/textures/oak_log.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Oak_Leaves, "resources/textures/oak_leaves.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Oak_Planks, "resources/textures/oak_planks.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Woodcutter, "resources/textures/woodcutter.png");    
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Fire, "resources/textures/fire.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Sand, "resources/textures/sand.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Coarse_Dirt, "resources/textures/coarse_dirt.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Snow, "resources/textures/snow.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Ruby_Ore, "resources/textures/ruby_ore.png");
    

    BlockAtlas::add(AssetManager::GameTextureID::Stone);
    BlockAtlas::add(AssetManager::GameTextureID::Grass);
    BlockAtlas::add(AssetManager::GameTextureID::Dirt);
    BlockAtlas::add(AssetManager::GameTextureID::Cobblestone);
    BlockAtlas::add(AssetManager::GameTextureID::Obsidian);
    BlockAtlas::add(AssetManager::GameTextureID::Bedrock);
    BlockAtlas::add(AssetManager::GameTextureID::Water);
    BlockAtlas::add(AssetManager::GameTextureID::Iron_Ore);
    BlockAtlas::add(AssetManager::GameTextureID::Gold_Ore);
    BlockAtlas::add(AssetManager::GameTextureID::Diamond_Ore);
    BlockAtlas::add(AssetManager::GameTextureID::Oak_Log);
    BlockAtlas::add(AssetManager::GameTextureID::Oak_Leaves);
    BlockAtlas::add(AssetManager::GameTextureID::Oak_Planks);
    BlockAtlas::add(AssetManager::GameTextureID::Woodcutter);
    BlockAtlas::add(AssetManager::GameTextureID::Fire);
    BlockAtlas::add(AssetManager::GameTextureID::Sand);
    BlockAtlas::add(AssetManager::GameTextureID::Coarse_Dirt);
    BlockAtlas::add(AssetManager::GameTextureID::Snow);
    BlockAtlas::add(AssetManager::GameTextureID::Ruby_Ore);
    BlockAtlas::build();

    AssetManager::loadShader(AssetManager::ShaderID::Sun, "resources/shaders/sun.frag");
    AssetManager::loadShader(AssetManager::ShaderID::Brightness, "resources/shaders/brightness.frag");

    AssetManager::loadSound(AssetManager::SoundID::Click, "resources/sounds/click.wav");

    AssetManager::loadMusic(AssetManager::MusicID::Dream, "resources/sounds/music/dream.mp3");

    

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
    handleBufferedStateActions();

    if(gameStates.empty()) return;

    for(auto& state : gameStates)
    {
        if(state == gameStates.back() || state->alwaysUpdated())
        {
            assert(state != nullptr);

            state->update(dt);
        }
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

void Game::pushState(GameState* sender, std::unique_ptr<GameState> state)
{
    if(gameStates.empty() || sender == gameStates.back().get())
    {
        actions_queue.push(GameStateAction{GameStateAction::TYPE::PUSH, std::move(state)});
        console.writeLine(L"Requested push of GameState");
    }
}

void Game::popState(GameState* sender)
{
    if(gameStates.empty() || sender != gameStates.back().get()) return;

    actions_queue.push(GameStateAction{GameStateAction::TYPE::POP, std::nullopt});
    console.writeLine(L"Requested pop of GameState");

}

void Game::handleBufferedStateActions()
{
    while(!actions_queue.empty())
    {
        switch(actions_queue.front().type)
        {
            case GameStateAction::TYPE::PUSH:

                if(!gameStates.empty()) gameStates.back()->onObscured();
                gameStates.emplace_back(std::move(*actions_queue.front().state));
                
                break;

            case GameStateAction::TYPE::POP:

                gameStates.pop_back();
                if(!gameStates.empty()) gameStates.back()->onRevealed();
                break;
        }

        actions_queue.pop();
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

void Game::popStates(GameState* sender, size_t amount)
{
    for(size_t i = 0; i < amount; i++)
    {
        popState(sender);
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