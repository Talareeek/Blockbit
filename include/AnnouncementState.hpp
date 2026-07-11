#ifndef ANNOUNCEMENT_STATE_HPP
#define ANNOUNCEMENT_STATE_HPP

#include "GameState.hpp"

class AnnouncementState : public GameState
{
private:

    sf::Texture background_texture;

    std::string text;

public:

    AnnouncementState(Game* game, std::string text);

    void handleEvent(const sf::Event& event) override;

    void update(float dt);

    void render(sf::RenderWindow& window);
};

#endif // ANNOUNCEMENT_STATE_HPP