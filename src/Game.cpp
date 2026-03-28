#include <thread>
#include <chrono>

#include "../include/Game.hpp"
#include "../include/Entity.hpp"
#include "../include/PhysicsSystem.hpp"
#include "../include/RenderSystem.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/AssetManager.hpp"
#include "../include/World.hpp"

Game::Game() : world(std::rand())
{
    window.create(sf::VideoMode::getDesktopMode(), "Blockbit");

    loadingScreen();

    unit_size = window.getSize().y / 9;
}

Entity& Game::entityWithID(uint32_t id)
{
    for(auto& a : entities)
    {
        if(id == a.getID()) return a;
    }
    throw std::runtime_error("Entity with ID " + std::to_string(id) + " does not exist(Game::entityWithID(int))");
}

void Game::loadingScreen()
{
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
    sf::Text text(AssetManager::getFont(0), "Loading...", window.getSize().y / 40);
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.draw(text);

    window.display();

    AssetManager::loadTexture(UINT32_MAX, "resources/textures/nothing.png");
    text.setString("Loading texture: resources/textures/nothing.png WTF :)");
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.clear();
    window.draw(background);
    window.draw(text);
    window.display();

    AssetManager::loadTexture(0, "resources/textures/player.png");
    text.setString("Loading texture: resources/textures/player.png");
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.clear();
    window.draw(background);
    window.draw(text);
    window.display();

    AssetManager::loadTexture(1, "resources/textures/stone.png");
    text.setString("Loading texture: resources/textures/stone.png");
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.clear();
    window.draw(background);
    window.draw(text);
    window.display();

    AssetManager::loadTexture(2, "resources/textures/grass.png");
    text.setString("Loading texture: resources/textures/grass.png");
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.clear();
    window.draw(background);
    window.draw(text);
    window.display();

    AssetManager::loadTexture(3, "resources/textures/dirt.png");
    text.setString("Loading texture: resources/textures/dirt.png");
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.clear();
    window.draw(background);
    window.draw(text);
    window.display();

    AssetManager::loadTexture(4, "resources/textures/cobblestone.png");
    text.setString("Loading texture: resources/textures/cobblestone.png");
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.clear();
    window.draw(background);
    window.draw(text);
    window.display();
    
    AssetManager::loadTexture(5, "resources/textures/obsidian.png");
    text.setString("Loading texture: resources/textures/obsidian.png");
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.clear();
    window.draw(background);
    window.draw(text);
    window.display();

    AssetManager::loadTexture(6, "resources/textures/bedrock.png");
    text.setString("Loading texture: resources/textures/bedrock.png");
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.clear();
    window.draw(background);
    window.draw(text);
    window.display();

    AssetManager::loadTexture(7, "resources/textures/coin.png");
    text.setString("Loading texture: resources/textures/coin.png");
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.clear();
    window.draw(background);
    window.draw(text);
    window.display();

    AssetManager::loadTexture(8, "resources/textures/overlay.png");
    text.setString("Loading texture: resources/textures/overlay.png");
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.clear();
    window.draw(background);
    window.draw(text);
    window.display();

    AssetManager::loadTexture(9, "resources/textures/inventory.png");
    text.setString("Loading texture: resources/textures/inventory.png");
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.clear();
    window.draw(background);
    window.draw(text);
    window.display();

    AssetManager::loadTexture(10, "resources/textures/hotbar.png");
    text.setString("Loading texture: resources/textures/hotbars.png");
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.clear();
    window.draw(background);
    window.draw(text);
    window.display();

    AssetManager::loadTexture(11, "resources/textures/water.png");
    text.setString("Loading texture: resources/textures/water.png");
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.clear();
    window.draw(background);
    window.draw(text);
    window.display();

    AssetManager::loadTexture(12, "resources/textures/dynamite.png");
    text.setString("Loading texture: resources/textures/dynamite.png");
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.clear();
    window.draw(background);
    window.draw(text);
    window.display();


    world.generateWorld();
    text.setString("Generating world...");
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.clear();
    window.draw(background);
    window.draw(text);
    window.display();

    std::filesystem::path savePath(std::getenv("APPDATA"));
    savePath /= "Blockbit";
    savePath /= "account";
    account = new Account(savePath);

    text.setString("Welcome to Blockbit!");
    text.setPosition({window.getSize().x / 2.0f - text.getLocalBounds().size.x / 2.0f, window.getSize().y / 2.0f - text.getLocalBounds().size.y / 2.0f});
    window.clear();
    window.draw(background);
    window.draw(text);
    window.display();
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
}

void Game::handleEvents()
{
    while(auto event = window.pollEvent())
    {
        if(event->is<sf::Event::Closed>())
        {
            window.close();
            WorldOutputStream(std::string("save.bin")).write(world);
        }

        else if(event->is<sf::Event::Resized>())
        {
            sf::View view(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(event->getIf<sf::Event::Resized>()->size.x), static_cast<float>(event->getIf<sf::Event::Resized>()->size.y)}));
            window.setView(view);
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
    }
}

void Game::update()
{
    if(!gameStates.empty())
    {
        gameStates.back()->update(dt);
    }
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

    window.display();
}

void Game::run()
{
    while(window.isOpen())
    {
        dt = std::min(clock.restart().asSeconds(), 0.05f);

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