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
#include "../include/Prefab.hpp"
#include "../include/Settings.hpp"

#include <imgui.h>
#include <imgui-SFML.h>

Game::Game()
{
    Settings::load();

    fullscreen = Settings::graphics.fullscreen;

    window.create(
        fullscreen ? sf::VideoMode::getDesktopMode()
                   : sf::VideoMode({Settings::graphics.window_width, Settings::graphics.window_height}),
        "Blockbit",
        fullscreen ? sf::State::Fullscreen : sf::State::Windowed
    );
    window.setVerticalSyncEnabled(Settings::graphics.vsync);
    if(Settings::graphics.fps_limit > 0) window.setFramerateLimit(Settings::graphics.fps_limit);
	
	if (!icon.loadFromFile("resources/textures/grass.png")) throw std::runtime_error("Failed loading an icon");
	
	window.setIcon(icon);

    if (!ImGui::SFML::Init(window)) throw std::runtime_error("Failed initializing ImGui-SFML");

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
    AssetManager::loadUITexture(AssetManager::UITextureID::Chest, "resources/textures/chest_view.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::Furnace, "resources/textures/furnace_view.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::Crafting, "resources/textures/crafting_view.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::Border, "resources/textures/border.png");
    AssetManager::getUITexture(AssetManager::UITextureID::Border).setRepeated(true);


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
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Ruby_Pickaxe, "resources/textures/ruby_pickaxe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Ruby_Axe, "resources/textures/ruby_axe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Ruby_Shovel, "resources/textures/ruby_shovel.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Ruby, "resources/textures/ruby.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Stick, "resources/textures/stick.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Coal, "resources/textures/coal.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Raw_Iron, "resources/textures/raw_iron.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Raw_Gold, "resources/textures/raw_gold.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Iron_Ingot, "resources/textures/iron_ingot.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Gold_Ingot, "resources/textures/gold_ingot.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Diamond, "resources/textures/diamond.png");

    AssetManager::loadGameTexture(AssetManager::GameTextureID::Stone, "resources/textures/stone.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Grass, "resources/textures/grass.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Dirt, "resources/textures/dirt.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Cobblestone, "resources/textures/cobblestone.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Obsidian, "resources/textures/obsidian.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Bedrock, "resources/textures/bedrock.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Water, "resources/textures/water.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Lava, "resources/textures/lava.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Coal_Ore, "resources/textures/coal_ore.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Iron_Ore, "resources/textures/iron_ore.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Gold_Ore, "resources/textures/gold_ore.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Diamond_Ore, "resources/textures/diamond_ore.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Iron_Block, "resources/textures/iron_block.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Gold_Block, "resources/textures/gold_block.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Ruby_Block, "resources/textures/ruby_block.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Coal_Block, "resources/textures/coal_block.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Oak_Log, "resources/textures/oak_log.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Oak_Leaves, "resources/textures/oak_leaves.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Oak_Planks, "resources/textures/oak_planks.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Woodcutter, "resources/textures/woodcutter.png");    
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Fire, "resources/textures/fire.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Sand, "resources/textures/sand.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Coarse_Dirt, "resources/textures/coarse_dirt.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Snow, "resources/textures/snow.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Ruby_Ore, "resources/textures/ruby_ore.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Chest, "resources/textures/chest.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Bricks, "resources/textures/bricks.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Furnace, "resources/textures/furnace.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Workbench, "resources/textures/workbench.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Gravel, "resources/textures/gravel.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Cactus, "resources/textures/cactus.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Flint, "resources/textures/flint.png");

    BlockAtlas::add(AssetManager::GameTextureID::Stone);
    BlockAtlas::add(AssetManager::GameTextureID::Grass);
    BlockAtlas::add(AssetManager::GameTextureID::Dirt);
    BlockAtlas::add(AssetManager::GameTextureID::Cobblestone);
    BlockAtlas::add(AssetManager::GameTextureID::Obsidian);
    BlockAtlas::add(AssetManager::GameTextureID::Bedrock);
    BlockAtlas::add(AssetManager::GameTextureID::Water);
    BlockAtlas::add(AssetManager::GameTextureID::Lava);
    BlockAtlas::add(AssetManager::GameTextureID::Coal_Ore);
    BlockAtlas::add(AssetManager::GameTextureID::Iron_Ore);
    BlockAtlas::add(AssetManager::GameTextureID::Gold_Ore);
    BlockAtlas::add(AssetManager::GameTextureID::Diamond_Ore);
    BlockAtlas::add(AssetManager::GameTextureID::Iron_Block);
    BlockAtlas::add(AssetManager::GameTextureID::Gold_Block);
    BlockAtlas::add(AssetManager::GameTextureID::Ruby_Block);
    BlockAtlas::add(AssetManager::GameTextureID::Coal_Block);
    BlockAtlas::add(AssetManager::GameTextureID::Oak_Log);
    BlockAtlas::add(AssetManager::GameTextureID::Oak_Leaves);
    BlockAtlas::add(AssetManager::GameTextureID::Oak_Planks);
    BlockAtlas::add(AssetManager::GameTextureID::Woodcutter);
    BlockAtlas::add(AssetManager::GameTextureID::Fire);
    BlockAtlas::add(AssetManager::GameTextureID::Sand);
    BlockAtlas::add(AssetManager::GameTextureID::Coarse_Dirt);
    BlockAtlas::add(AssetManager::GameTextureID::Snow);
    BlockAtlas::add(AssetManager::GameTextureID::Ruby_Ore);
    BlockAtlas::add(AssetManager::GameTextureID::Chest);
    BlockAtlas::add(AssetManager::GameTextureID::Bricks);
    BlockAtlas::add(AssetManager::GameTextureID::Furnace);
    BlockAtlas::add(AssetManager::GameTextureID::Workbench);
    BlockAtlas::add(AssetManager::GameTextureID::Gravel);
    BlockAtlas::add(AssetManager::GameTextureID::Cactus);
    BlockAtlas::build();

    AssetManager::loadShader(AssetManager::ShaderID::Sun, "resources/shaders/sun.frag");
    AssetManager::loadShader(AssetManager::ShaderID::Brightness, "resources/shaders/brightness.frag");
    AssetManager::loadShader(AssetManager::ShaderID::BlockAnimation, "resources/shaders/block_animation.frag");
    AssetManager::loadShader(AssetManager::ShaderID::LightingOverlay, "resources/shaders/lighting_overlay.frag");

    AssetManager::loadSound(AssetManager::SoundID::Click, "resources/sounds/click.wav");

    AssetManager::loadMusic(AssetManager::MusicID::Dream, "resources/sounds/music/dream.mp3");

    PrefabRegistry::init();


    console.assignGame(this);
    console.setupFont("resources/fonts/FiraCode-Regular.ttf");

    settings_window.assignGame(this);
}

void Game::handleEvents()
{
    while(auto event = window.pollEvent())
    {
        try
        {
            ImGui::SFML::ProcessEvent(window, *event);

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
                if(event->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::F11)
                {
                    Settings::graphics.fullscreen = !Settings::graphics.fullscreen;
                    applyGraphicsSettings();
                    Settings::save();
                }
                else if(event->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Grave)
                {
                    console_visible = !console_visible;
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
    ImGui::SFML::Update(window, sf::seconds(dt));

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
            if(state->renderWhenObscured() || gameStates.back() == state) state->render(window);
        }
    }
    else
    {
        exit(0);
    }

    console.render();
    settings_window.render();

    ImGui::SFML::Render(window);

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

        if(settings_window.consumeAudioRequest())    applyAudioSettings();
        if(settings_window.consumeGraphicsRequest()) applyGraphicsSettings();
    }

    ImGui::SFML::Shutdown();
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

SettingsWindow& Game::getSettingsWindow()
{
    return settings_window;
}

void Game::applyGraphicsSettings()
{
    fullscreen = Settings::graphics.fullscreen;

    window.close();
    ImGui::SFML::Shutdown(window);

    window.create(
        fullscreen ? sf::VideoMode::getDesktopMode()
                   : sf::VideoMode({Settings::graphics.window_width, Settings::graphics.window_height}),
        "Blockbit",
        fullscreen ? sf::State::Fullscreen : sf::State::Windowed
    );

    window.setVerticalSyncEnabled(Settings::graphics.vsync);
    window.setFramerateLimit(Settings::graphics.fps_limit);

    if(!ImGui::SFML::Init(window)) throw std::runtime_error("Failed re-initializing ImGui-SFML");

    console.setupFont("resources/fonts/FiraCode-Regular.ttf");

    sf::View view(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)}));
    window.setView(view);
    window.setIcon(icon);
}

void Game::applyAudioSettings()
{
    float master = Settings::audio.master_volume / 100.0f;
    jukebox.setMusicVolume(Settings::audio.music_volume * master);
    jukebox.setSoundsVolume(Settings::audio.sound_volume * master);
}