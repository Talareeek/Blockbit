#ifndef CREATE_WORLD_STATE_HPP
#define CREATE_WORLD_STATE_HPP

#include "GameState.hpp"
#include "FlatWorldGenerator.hpp"

#include <SFML/Graphics/Texture.hpp>

#include <vector>

class CreateWorldState : public GameState
{
private:

    enum class Mode : int
    {
        Normal         = 0,
        Flat           = 1,
        DungeonShooter = 2
    };

    Mode mode = Mode::Normal;

    char name_buffer[64] = {};
    char seed_buffer[32] = {};

    float base_height  = 55.0f;
    float height_scale = 35.0f;
    float frequency    = 0.035f;
    float amplitude    = 1.0f;
    float persistence  = 0.55f;

    std::vector<FlatWorldLayer> flat_layers;

    sf::Texture background_texture;

    bool should_close = false;

    void createWorld();

public:

    CreateWorldState(Game* game);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // CREATE_WORLD_STATE_HPP
