#include "../include/MainGameState.hpp"
#include "../include/Game.hpp"
#include "../include/Entity.hpp"
#include "../include/PhysicsSystem.hpp"
#include "../include/RenderSystem.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/AssetManager.hpp"
#include "../include/World.hpp"
#include "../include/PauseScreenState.hpp"
#include "../include/HealthComponent.hpp"

#include <iostream>

Entity& entityWithID(uint32_t id, World& world)
{
    for(auto& a : world.getEntities())
    {
        if(id == a.getID()) return a;
    }
    throw std::runtime_error("Entity with ID " + std::to_string(id) + " does not exist(entityWithID(int, World&))");
}

MainGameState::MainGameState(Game* game) : GameState(game)
{
    world = World(std::rand());
    world.generateWorld();    

    auto& entities = world.getEntities();

    entities.emplace_back(1);

    entityWithID(1, world).addComponent(PhysicsComponent{{0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});

    for(int i = 0; i < 255; i++)
    {
        if(world.getBlock(0, i) == BlockID::Air)
        {
            entityWithID(1, world).getComponent<PhysicsComponent>().position.y = i + 1.0f;
            break;
        }
    }

    entityWithID(1, world).addComponent(RenderComponent{0, {{0, 0}, {16, 16}}, {1.0f, 1.0f}});

    entityWithID(1, world).addComponent(HealthComponent{100, 100});

    healthBar = HealthBar({10.0f, 10.0f}, {200.0f, 20.0f});
}

void MainGameState::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::Resized>())
    {
        auto resized = event.getIf<sf::Event::Resized>();

        healthBar.setSize({static_cast<float>(resized->size.x) / 5.0f, static_cast<float>(resized->size.y) / 20.0f});

        healthBar.setPosition({static_cast<float>(resized->size.x) / 40.0f, static_cast<float>(resized->size.y) - (static_cast<float>(resized->size.y) / 40.0f) - healthBar.getSize().y});
    }
    else if(event.is<sf::Event::MouseButtonPressed>())
    {
        auto mouse = event.getIf<sf::Event::MouseButtonPressed>();

        if(mouse->button == sf::Mouse::Button::Left)
        {
            float unit_size = game->getWindow().getSize().y / 9.0f;

            sf::View view({(entityWithID(1, world).getComponent<PhysicsComponent>().position.x + 0.5f) * unit_size, worldToScreenY(entityWithID(1, world).getComponent<PhysicsComponent>().position.y - 0.5f, unit_size, game->getWindow().getSize().y)},{static_cast<float>(game->getWindow().getSize().x), static_cast<float>(game->getWindow().getSize().y)});

            game->getWindow().setView(view);

            sf::Vector2i blockPos = getMouseBlockPosition(world, game->getWindow());

            if(world.getBlock(blockPos.x, blockPos.y) != BlockID::Air)
            {
                world.setBlock(blockPos.x, blockPos.y, BlockID::Air);
            }
        }
        else if(mouse->button == sf::Mouse::Button::Right)
        {
            float unit_size = game->getWindow().getSize().y / 9.0f;

            sf::View view({(entityWithID(1, world).getComponent<PhysicsComponent>().position.x + 0.5f) * unit_size, worldToScreenY(entityWithID(1, world).getComponent<PhysicsComponent>().position.y - 0.5f, unit_size, game->getWindow().getSize().y)},{static_cast<float>(game->getWindow().getSize().x), static_cast<float>(game->getWindow().getSize().y)});

            game->getWindow().setView(view);

            sf::Vector2i blockPos = getMouseBlockPosition(world, game->getWindow());

            if(world.getBlock(blockPos.x, blockPos.y) == BlockID::Air)
            {
                world.setBlock(blockPos.x, blockPos.y, BlockID::Stone);
            }
        }
    }

    healthBar.handleEvent(event);

    inventoryWidget.handleEvent(event);
}

void MainGameState::update(float dt)
{
    auto& entities = world.getEntities();

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) && entityWithID(1, world).getComponent<PhysicsComponent>().onGround)
    {
        auto& component = entityWithID(1, world).getComponent<PhysicsComponent>();
        component.velocity.y += 10.0f;
    }
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
    {
        auto& component = entityWithID(1, world).getComponent<PhysicsComponent>();
        component.force.x -= 45.0f;

        auto& render = entityWithID(1, world).getComponent<RenderComponent>();
        render.uv = {{0, 32}, {16, 16}};
    }
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
    {
        auto& component = entityWithID(1, world).getComponent<PhysicsComponent>();
        component.force.x += 45.0f;

        auto& render = entityWithID(1, world).getComponent<RenderComponent>();
        render.uv = {{32, 32}, {16, 16}};
    }
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape))
    {
        game->pushState(std::make_unique<PauseScreenState>(game));
    }

    PhysicsSystem(entities, world, dt);

    healthBar.setHealth(entityWithID(1, world).getComponent<HealthComponent>());

    healthBar.update(dt);

    inventoryWidget.update(dt);

    world.tick(dt);
}

void MainGameState::render(sf::RenderWindow& window)
{
    auto& entities = world.getEntities();

    window.clear(world.getSkyColor(world.getDayTime() / World::DAY_CYCLE_DURATION));

    unsigned int unit_size = window.getSize().y / 9;

    sf::View view(
        {(entityWithID(1, world).getComponent<PhysicsComponent>().position.x + 0.5f) * unit_size, worldToScreenY(entityWithID(1, world).getComponent<PhysicsComponent>().position.y - 0.5f, unit_size, window.getSize().y)},
        {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)}
    );

    window.setView(view);
    
    RenderWorld(world, window);

    RenderSystem(entities, window);

    RenderBlockOverlay(world, window);

    sf::View defaultView = window.getDefaultView();
    window.setView(defaultView);

    healthBar.render(window);

    inventoryWidget.render(window);
}