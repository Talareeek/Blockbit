#include "../include/MenuGameState.hpp"
#include "../include/MainGameState.hpp"
#include "../include/Game.hpp"
#include "../include/AssetManager.hpp"

MenuGameState::MenuGameState(Game* game) : GameState(game)
{
    UIElement::ScreenRelative playRelative{{0.3f, 0.25f}, {0.4f, 0.2f}, true, UIElement::ScreenRelative::Axis::Y};
    UIElement::ScreenRelative quitRelative{{0.3f, 0.55f}, {0.4f, 0.2f}, true, UIElement::ScreenRelative::Axis::Y};

    play = Button(playRelative, sf::Color::Green, "Create World", [this]()
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
        const char* appdata = std::getenv("APPDATA");
        home = appdata ? appdata : "";
    #elif __linux__
        const char* homeenv = std::getenv("HOME");
        home = homeenv ? homeenv : "";
    #endif

    std::filesystem::path savesPath = home.empty() ? std::filesystem::temp_directory_path() : std::filesystem::path(home);
    savesPath /= "Blockbit";
    savesPath /= "saves";
    
    std::filesystem::create_directories(savesPath);
    worldList = WorldList(savesPath);
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


    sf::Text copyright(AssetManager::getFont(0), L"©2026 Talarek\n(github.com/Talareeek)", 10);
    copyright.setPosition({5.0f, 5.0f});
    copyright.setFillColor(sf::Color(0, 0, 0, 127));
    copyright.setOutlineColor(sf::Color(255, 255, 255, 127));
    copyright.setOutlineThickness(1.0f);

    window.draw(copyright);
}