#include "../include/MenuGameState.hpp"
#include "../include/MainGameState.hpp"
#include "../include/Game.hpp"
#include "../include/AssetManager.hpp"

MenuGameState::MenuGameState(Game* game) : GameState(game)
{
    UIElement::ScreenRelative playRelative{{0.3f, 0.25f}, {0.4f, 0.2f}, true, UIElement::ScreenRelative::Axis::Y};
    UIElement::ScreenRelative quitRelative{{0.3f, 0.55f}, {0.4f, 0.2f}, true, UIElement::ScreenRelative::Axis::Y};

    play = Button(playRelative, sf::Color::Green, "Play", [this]()
    {
        this->game->pushState(std::make_unique<MainGameState>(this->game));
    });

    play.updateScreenRelative(game->getWindow().getSize());

    quit = Button(quitRelative, sf::Color::Red, "Quit", [this]()
    {
        this->game->popState();
    });

    quit.updateScreenRelative(game->getWindow().getSize());

    std::string home;

    #ifdef _WIN32
        home = std::getenv("APPDATA");
    #elif __linux__
        home = std::getenv("HOME");
    #endif

    worldList = WorldList(std::filesystem::path(home) / "Blockbit" / "saves");
}

void MenuGameState::handleEvent(const sf::Event& event)
{
    play.handleEvent(event);
    quit.handleEvent(event);
    worldList.handleEvent(event);
}

void MenuGameState::update(float dt)
{
    auto size = game->getWindow().getSize();

    play.updateScreenRelative(size);
    quit.updateScreenRelative(size);
    worldList.updateScreenRelative(size);

    worldList.setPosition({size.x * 0.75f, 0.0f});
    worldList.setSize({size.x * 0.25f, static_cast<float>(size.y)});

    play.update(dt);
    quit.update(dt);
    worldList.update(dt);
}

void MenuGameState::render(sf::RenderWindow& window)
{
    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));

    sf::Sprite background(AssetManager::getTexture(200135));
    background.setPosition({0.0f, 0.0f});
    background.setScale
    (
        {
            static_cast<float>(window.getSize().x) / background.getTexture().getSize().x,
            static_cast<float>(window.getSize().y) / background.getTexture().getSize().y
        }
    );
    window.draw(background);

    sf::RectangleShape overlay({static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)});
    overlay.setFillColor(sf::Color(0, 0, 0, 150));
    window.draw(overlay);

    worldList.render(window);
    play.render(window);
    quit.render(window);    
}