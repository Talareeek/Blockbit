#ifndef MAIN_GAME_STATE_HPP
#define MAIN_GAME_STATE_HPP

#include "GameState.hpp"
#include "World.hpp"
#include "HealthBar.hpp"
#include "InventoryWidget.hpp"
#include "Hotbar.hpp"
#include "Button.hpp"
#include "TransformComponent.hpp"
#include "Input.hpp"

extern Entity& entityWithID(uint32_t id, World& world);

class MainGameState : public GameState
{
protected:

    World world;
    HealthBar healthBar;

    InventoryWidget inventoryWidget{nullptr};
    Hotbar hotbar;

    bool debug = false;

    int fps = 0;
    float last_fps_update = 1.0f;

    float music_interval = std::rand() % 60 + 60.0f;
    float music_timer = 0.0f;

    float since_last_tick = 0.0f;

    std::vector<Input> inputs;

public:

    World& getWorld() { return world; }

    MainGameState(Game* game, World world);

    ~MainGameState() override;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    std::string debugString();

    std::vector<Input> getInputs();
    std::vector<Input> getInputsFromEvent(const sf::Event& event);

    void processInputs(std::vector<Input> inputs, uint32_t id);

    static constexpr unsigned int UNIT_SIZE_FACTOR = 12;

    static constexpr uint8_t TICKS_PER_SECOND = 60;

    

};

extern bool isInRange(TransformComponent& player, TransformComponent& target, float range);
extern bool isBlockInRange(TransformComponent& player, sf::Vector2i& block, float range);

#endif // MAIN_GAME_STATE_HPP