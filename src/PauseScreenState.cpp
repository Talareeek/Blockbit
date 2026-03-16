#include "../include/PauseScreenState.hpp"
#include "../include/Game.hpp"
#include "../include/AssetManager.hpp"

PauseScreenState::PauseScreenState(Game* game) : GameState(game)
{
    resume = Button({50.0f, 50.0f}, {250.0f, 50.0f}, sf::Color::Green, "Resume", [this](){ this->game->popState(); });
    quit = Button({50.0f, 120.0f}, {250.0f, 50.0f}, sf::Color::Red, "Quit", [this](){ this->game->popStates(2);});
}

void PauseScreenState::handleEvent(const sf::Event& event)
{
    resume.handleEvent(event);
    quit.handleEvent(event);
}

void PauseScreenState::update(float dt)
{
    resume.update(dt);
    quit.update(dt);
}

void PauseScreenState::render(sf::RenderWindow& window)
{
    window.setView(window.getDefaultView());

    sf::RectangleShape background;
    background.setPosition({0.0f, 0.0f});
    background.setSize({static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)});
    background.setFillColor(sf::Color(255, 255, 255, static_cast<unsigned char>(255 * 0.25f)));
    window.draw(background);

    resume.render(window);
    quit.render(window);
}