#include "../include/LobbyPlayerElement.hpp"
#include "../include/AssetManager.hpp"

constexpr int HEIGHT_FACTOR = 5;
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
    constexpr int FRAME_W = 12;
    constexpr int FRAME_H = 16;

    const float h = static_cast<float>(window.getSize().y) / static_cast<float>(HEIGHT_FACTOR);
    const float w = h * static_cast<float>(FRAME_W) / static_cast<float>(FRAME_H);

    size = {w, h};
    position = {(static_cast<float>(window.getSize().x) - size.x) / 2.0f, (static_cast<float>(window.getSize().y) - size.y) / 2.0f};

    sf::RectangleShape rect(size);
    rect.setPosition(position);

    rect.setTexture(&AssetManager::getGameTexture(AssetManager::GameTextureID::Player));

    // player.png is 48x16: idle frames 0,1 at x=0,12 and walking frames 2,3 at x=24,36.
    // Walking frames natively face left; RIGHT mirrors them via a negative-width texture rect.
    const int idle_x = step ? 0 : FRAME_W;
    const int walk_x = step ? 2 * FRAME_W : 3 * FRAME_W;

    sf::IntRect texture_rect;

    switch(direction)
    {
    case Direction::CENTER:
        texture_rect = {{idle_x, 0}, {FRAME_W, FRAME_H}};
        break;

    case Direction::LEFT:
        texture_rect = {{walk_x, 0}, {FRAME_W, FRAME_H}};
        break;

    case Direction::RIGHT:
        texture_rect = {{walk_x + FRAME_W, 0}, {-FRAME_W, FRAME_H}};
        break;
    }

    rect.setTextureRect(texture_rect);

    window.draw(rect);
}