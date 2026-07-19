#include "../include/AnnouncementState.hpp"
#include "../include/Render.hpp"
#include "../include/InputManager.hpp"
#include "../include/Game.hpp"
#include "../include/AssetManager.hpp"

AnnouncementState::AnnouncementState(Game* game, std::string text) : GameState(game), text{text}
{
    background_texture = generateBackground();
}

void AnnouncementState::handleEvent(const sf::Event& event)
{

}

void AnnouncementState::update(float dt)
{
    if(InputManager::isLazyKeyPressed(sf::Keyboard::Key::Escape))
    {
        game->popState(this);
    }
}

void AnnouncementState::render(sf::RenderWindow& window)
{
    sf::Sprite background(background_texture);
    background.setPosition({0.0f, 0.0f});
    background.setScale
    (
        {
            static_cast<float>(window.getSize().x) / background.getTexture().getSize().x,
            static_cast<float>(window.getSize().y) / background.getTexture().getSize().y
        }
    );
    window.draw(background);


    sf::Text text(AssetManager::getFont(0), this->text, 20);

    text.setPosition({10.0f, 10.0f});

    window.draw(text);

}