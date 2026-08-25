#include "../include/LobbyPlayerElement.hpp"
#include "../include/AssetManager.hpp"

constexpr int HEIGHT_FACTOR = 5;
constexpr uint32_t PLAYER_TEXTURE_ID = 0;
constexpr float STEP_LENGTH = 0.5f;

LobbyPlayerElement::LobbyPlayerElement() : UIElement({0.0f, 0.0f}, {0.0f, 0.0f})
{

}

void LobbyPlayerElement::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::MouseMoved>())
    {
        auto mouse = event.getIf<sf::Event::MouseMoved>();

        float mouse_x = static_cast<float>(mouse->position.x);

        if(mouse_x < position.x) direction = Direction::LEFT;
        else if(position.x + size.x < mouse_x) direction = Direction::RIGHT;
        else direction = Direction::CENTER;
    }
}

void LobbyPlayerElement::update(float dt)
{
    last_step += dt;

    while(last_step >= STEP_LENGTH)
    {
        last_step -= STEP_LENGTH;
        step = !step;
    }
}

void LobbyPlayerElement::render(sf::RenderWindow& window)
{
    size = {static_cast<float>(window.getSize().y) / static_cast<float>(HEIGHT_FACTOR), static_cast<float>(window.getSize().y) / static_cast<float>(HEIGHT_FACTOR)};
    position = {(static_cast<float>(window.getSize().x) - size.x) / 2.0f, (static_cast<float>(window.getSize().y) - size.y) / 2.0f};

    sf::RectangleShape rect(size);
    rect.setPosition(position);

    rect.setTexture(&AssetManager::getGameTexture(AssetManager::GameTextureID::Player));

    sf::IntRect texture_rect;

    switch(direction)
    {
    case Direction::LEFT:

        if(step) texture_rect = {{0, 32}, {16, 16}};
        else texture_rect = {{32, 16}, {16, 16}};

        break;

    case Direction::CENTER:

        if(step) texture_rect = {{0, 16}, {16, 16}};
        else texture_rect = {{16, 16}, {16, 16}};

        break;

    case Direction::RIGHT:

        if(step) texture_rect = {{32, 32}, {16, 16}};
        else texture_rect = {{16, 32}, {16, 16}};

        break;
    }

    rect.setTextureRect(texture_rect);

    window.draw(rect);
}