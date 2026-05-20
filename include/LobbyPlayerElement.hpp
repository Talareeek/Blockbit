#ifndef LOBBY_PLAYER_ELEMENT_HPP
#define LOBBY_PLAYER_ELEMENT_HPP

#include "UIElement.hpp"

class LobbyPlayerElement : public UIElement
{
private:

    enum class Direction
    {
        LEFT,
        RIGHT,
        CENTER
    } direction = Direction::CENTER;

    bool step = true;

    float last_step = 0.0f;

public:

    LobbyPlayerElement();

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

};

#endif // LOBBY_PLAYER_ELEMENT_HPP