#ifndef CREATE_WORLD_STATE_HPP
#define CREATE_WORLD_STATE_HPP

#include "GameState.hpp"
#include "InputField.hpp"
#include "Button.hpp"
#include "Checkbox.hpp"
#include "GenerationPreview.hpp"
#include "World.hpp"

#include <optional>

class CreateWorldState : public GameState
{
private:

    Button quit;

    InputField name;
    InputField seed;

    Checkbox flat;

    Button create;

    GenerationPreview preview;

    std::optional<World> pending_world;

public:

    CreateWorldState(Game* game);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // CREATE_WORLD_STATE_HPP