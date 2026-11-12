#ifndef CREATE_WORLD_STATE_HPP
#define CREATE_WORLD_STATE_HPP

#include "GameState.hpp"
#include "FlatWorldGenerator.hpp"

#include <SFML/Graphics/Texture.hpp>

#include <string>
#include <unordered_map>
#include <vector>

class CreateWorldState : public GameState
{
private:

    enum class Mode : int
    {
        Normal         = 0,
        Flat           = 1,
        DungeonShooter = 2,
        Skygrid        = 3,
        Randomizer     = 4,
        Noodle         = 5
    };

    Mode mode = Mode::Normal;

    char name_buffer[64] = "New world";
    char seed_buffer[32] = {};

    float base_height  = 55.0f;
    float height_scale = 35.0f;
    float frequency    = 0.035f;
    float amplitude    = 1.0f;
    float persistence  = 0.55f;

    float noodle_threshold = 0.55f;

    bool spawn_item_chest_column = false;

    std::vector<FlatWorldLayer> flat_layers;

    sf::Texture background_texture;

    sf::Texture preview_texture;
    bool preview_has_image = false;
    unsigned int preview_seed = 0;

    bool should_close = false;

    // Per-pack enable state for this world. Initialised from the live
    // gameRegistries().loaded_packs — vanilla's "blockbit" is forced on;
    // modpack ids default to on so the common case (create world with all
    // installed packs) stays one click.
    std::unordered_map<std::string, bool> pack_enabled;

    void createWorld();

    void rerollSeed();
    void rebuildPreview();

public:

    CreateWorldState(Game* game);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // CREATE_WORLD_STATE_HPP
