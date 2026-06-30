#ifndef CREATE_WORLD_STATE_HPP
#define CREATE_WORLD_STATE_HPP

#include "GameState.hpp"
#include "InputField.hpp"
#include "Button.hpp"
#include "GenerationPreview.hpp"

class CreateWorldState : public GameState
{
private:

    InputField name;
    InputField seed;

    Button create;

    GenerationPreview preview;

public:

    CreateWorldState(Game* game);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // CREATE_WORLD_STATE_HPP