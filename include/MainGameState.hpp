#ifndef MAIN_GAME_STATE_HPP
#define MAIN_GAME_STATE_HPP

#include "GameState.hpp"
#include "World.hpp"
#include "HealthBar.hpp"
#include "InventoryWidget.hpp"
#include "Hotbar.hpp"
#include "Button.hpp"
#include "TransformComponent.hpp"

extern Entity& entityWithID(uint32_t id, World& world);

class MainGameState : public GameState
{
private:

    World world;
    HealthBar healthBar;

    InventoryWidget inventoryWidget{nullptr};
    Hotbar hotbar;

    bool debug = false;

    int fps = 0;
    float last_fps_update = 1.0f;

public:

    MainGameState(Game* game, World world);

    ~MainGameState() override;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;    

    static constexpr unsigned int UNIT_SIZE_FACTOR = 12;

};

extern bool isInRange(TransformComponent& player, TransformComponent& target, float range);
extern bool isBlockInRange(TransformComponent& player, sf::Vector2i& block, float range);

#endif // MAIN_GAME_STATE_HPP