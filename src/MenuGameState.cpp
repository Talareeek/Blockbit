#include "../include/MenuGameState.hpp"
#include "../include/MainGameState.hpp"
#include "../include/Game.hpp"
#include "../include/AssetManager.hpp"

MenuGameState::MenuGameState(Game* game) : GameState(game)
{
    UIElement::ScreenRelative quitRelative{{0.01f, 0.92f}, {0.12f, 0.06f}, UIElement::ScreenRelative::ScaleMode::UniformByHeight};

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
    worldList = WorldList(savesPath, game);

    quit = Button(quitRelative, sf::Color::Red, "Quit", [this]()
    {
        this->game->popState();
    });

    quit.updateScreenRelative(game->getWindow().getSize());

    UIElement::ScreenRelative sliderRelative{{0.30f, 0.45f}, {0.30f, 0.08f}, UIElement::ScreenRelative::ScaleMode::UniformByHeight};
    testSlider = Slider(sliderRelative, 0.0f, 100.0f, 50.0f, "Volume");
    testSlider.updateScreenRelative(game->getWindow().getSize());
}

void MenuGameState::handleEvent(const sf::Event& event)
{
    worldList.handleEvent(event);
    quit.handleEvent(event);
    player.handleEvent(event);
    testSlider.handleEvent(event);
}

void MenuGameState::update(float dt)
{
    auto size = game->getWindow().getSize();

    quit.updateScreenRelative(size);
    worldList.updateScreenRelative(size);
    testSlider.updateScreenRelative(size);

    worldList.setPosition({size.x * 0.75f, 0.0f});
    worldList.setSize({size.x * 0.25f, static_cast<float>(size.y)});

    worldList.update(dt);
    quit.update(dt);

    player.update(dt);
    testSlider.update(dt);
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
    overlay.setFillColor(sf::Color(255, 255, 255, 75));
    window.draw(overlay);

    worldList.render(window);
    quit.render(window);
    player.render(window);
    testSlider.render(window);


    sf::Text copyright(AssetManager::getFont(0), L"©2026 Talarek\n(github.com/Talareeek)", 10);
    copyright.setPosition({5.0f, 5.0f});
    copyright.setFillColor(sf::Color(0, 0, 0, 127));
    copyright.setOutlineColor(sf::Color(255, 255, 255, 127));
    copyright.setOutlineThickness(1.0f);

    window.draw(copyright);
}