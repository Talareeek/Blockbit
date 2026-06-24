#include "../include/AuthorGameState.hpp"
#include "../include/Game.hpp"
#include "../include/IntroGameState.hpp"
#include "../include/AssetManager.hpp"

void AuthorGameState::update(float dt)
{
    length += dt;

    if(AuthorGameState::ANIMATION_LENGTH <= length)
    {
        Game* temp_game = game;

        game->popState();

        temp_game->pushState(std::make_unique<IntroGameState>(temp_game));
    }
}

void AuthorGameState::handleEvent(const sf::Event& event)
{

}

void AuthorGameState::render(sf::RenderWindow& window)
{
    window.setTitle("Blockbit by Talarek");

    sf::RectangleShape logo;
    logo.setTexture(&AssetManager::getTexture(23));

    logo.setSize({static_cast<float>(logo.getTexture()->getSize().x), static_cast<float>(logo.getTexture()->getSize().y)});

    float factor = (static_cast<float>(window.getSize().x) / 3.0f) / static_cast<float>(logo.getTexture()->getSize().x);
    logo.setScale({factor, factor});

    logo.setPosition({(static_cast<float>(window.getSize().x) - static_cast<float>(logo.getGlobalBounds().size.x)) / 2.0f, (static_cast<float>(window.getSize().y) - static_cast<float>(logo.getGlobalBounds().size.y)) / 2.0f});

    window.draw(logo);
}

//23