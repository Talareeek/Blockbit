#include "../include/MenuGameState.hpp"
#include "../include/MainGameState.hpp"
#include "../include/Game.hpp"
#include "../include/AssetManager.hpp"

MenuGameState::MenuGameState(Game* game) : GameState(game)
{
    play = Button({50.0f, 50.0f}, {200.0f, 50.0f}, sf::Color::Green, "Play", [this]()
    {
        this->game->pushState(std::make_unique<MainGameState>(this->game));
    });

    quit = Button({50.0f, 100.0f}, {200.0f, 50.0f}, sf::Color::Red, "Quit", [this]()
    {
        this->game->popState();
    });

    accountWidget = AccountWidget(game->getAccount());
}

void MenuGameState::handleEvent(const sf::Event& event)
{
    play.handleEvent(event);
    quit.handleEvent(event);
    accountWidget.handleEvent(event);
}

void MenuGameState::update(float dt)
{
    play.update(dt);
    quit.update(dt);
    accountWidget.update(dt);
}

void MenuGameState::render(sf::RenderWindow& window)
{
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

    play.render(window);
    quit.render(window);
    accountWidget.render(window);
}