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
#include "../include/ItemComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/InventorySystem.hpp"
#include "../include/InventoryWidget.hpp"
#include "../include/InputManager.hpp"
#include "../include/DeathScreenState.hpp"

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
    // WORLD GENERATION
    world = World(std::rand());
    world.generateWorld();    

    auto& entities = world.getEntities();

    entities.emplace_back(1);

    entityWithID(1, world).addComponent(TransformComponent{{0.0f, 0.0f}, {1.0f, 1.0f}, sf::degrees(0.0f)});
    entityWithID(1, world).addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});
    entityWithID(1, world).addComponent(InventoryComponent(36));

    for(int i = 0; i < 255; i++)
    {
        if(world.getBlock(0, i).id == BlockID::Air)
        {
            entityWithID(1, world).getComponent<TransformComponent>().position.y = i + 1.0f;
            break;
        }
    }

    entityWithID(1, world).addComponent(RenderComponent{0, {{0, 0}, {16, 16}}, {1.0f, 1.0f}});

    entityWithID(1, world).addComponent(HealthComponent{100, 100});

    healthBar = HealthBar(UIElement::ScreenRelative{{0.05f, 0.05f}, {0.2f, 0.05f}, true, UIElement::ScreenRelative::Axis::Y});
    healthBar.updateScreenRelative(game->getWindow().getSize());

    inventoryWidget = InventoryWidget(&entityWithID(1, world).getComponent<InventoryComponent>());
    inventoryWidget.updateScreenRelative(game->getWindow().getSize());

    hotbar = Hotbar(&entityWithID(1, world).getComponent<InventoryComponent>());
    hotbar.updateScreenRelative(game->getWindow().getSize());
}

void MainGameState::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::MouseButtonPressed>())
    {
        auto mouse = event.getIf<sf::Event::MouseButtonPressed>();

        if(mouse->button == sf::Mouse::Button::Left)
        {
            float unit_size = game->getWindow().getSize().y / 9.0f;

            sf::View view(
            {
                (entityWithID(1, world).getComponent<TransformComponent>().position.x + 0.5f) * unit_size,
                (entityWithID(1, world).getComponent<TransformComponent>().position.y - 0.5f) * unit_size
            },
            {
                (float)game->getWindow().getSize().x,
                (float)game->getWindow().getSize().y
            });

            view.setSize({view.getSize().x, -view.getSize().y});            

            game->getWindow().setView(view);

            sf::Vector2i blockPos = getMouseBlockPosition(world, game->getWindow());

            if(world.getBlock(blockPos.x, blockPos.y).id != BlockID::Air && blockDatabase[world.getBlock(blockPos.x, blockPos.y).id].breakable)
            {
                world.getEntities().push_back(Entity(world.getEntities().size() + 1));
                auto& newEntity = world.getEntities().back();
                newEntity.addComponent(TransformComponent{{blockPos.x + 0.25f, blockPos.y - 0.25f}, {0.5f, 0.5f}, sf::degrees(0.0f)});
                newEntity.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, false, false, false, true});
                newEntity.addComponent(ItemComponent{{static_cast<ItemID>(world.getBlock(blockPos.x, blockPos.y).id), 1}});
                newEntity.addComponent(RenderComponent{static_cast<unsigned short>(itemDatabase[newEntity.getComponent<ItemComponent>().item.itemID].texture), {{0, 0}, {16, 16}}, {0.5f, 0.5f}});
                world.setBlock(blockPos.x, blockPos.y, {BlockID::Air, 0});
            }
        }
        else if(mouse->button == sf::Mouse::Button::Right)
        {
            float unit_size = game->getWindow().getSize().y / 9.0f;

            sf::View view(
            {
                (entityWithID(1, world).getComponent<TransformComponent>().position.x + 0.5f) * unit_size,
                (entityWithID(1, world).getComponent<TransformComponent>().position.y - 0.5f) * unit_size
            },
            {
                (float)game->getWindow().getSize().x,
                (float)game->getWindow().getSize().y
            });

            view.setSize({view.getSize().x, -view.getSize().y});

            game->getWindow().setView(view);

            sf::Vector2i blockPos = getMouseBlockPosition(world, game->getWindow());

            if(world.getBlock(blockPos.x, blockPos.y).id == BlockID::Air || world.getBlock(blockPos.x, blockPos.y).id == BlockID::Water)
            {
                world.setBlock(blockPos.x, blockPos.y, {BlockID::Stone, 0});
            }
        }
    }

    healthBar.handleEvent(event);

    inventoryWidget.handleEvent(event);

    hotbar.handleEvent(event);
}

void MainGameState::update(float dt)
{
    auto& entities = world.getEntities();


    // PLAYER MOVEMENT
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
    InventorySystem(entities);

    healthBar.setHealth(entityWithID(1, world).getComponent<HealthComponent>());

    healthBar.updateScreenRelative(game->getWindow().getSize());
    inventoryWidget.updateScreenRelative(game->getWindow().getSize());
    hotbar.updateScreenRelative(game->getWindow().getSize());

    healthBar.update(dt);
    hotbar.update(dt);

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E))
    {
        inventoryWidget.setActive(!inventoryWidget.isActive());
    }

    if(inventoryWidget.isActive())
    {
        inventoryWidget.update(dt);
    }

    world.tick(dt);

    if(entityWithID(1, world).getComponent<HealthComponent>().health <= 0)
    {
        game->pushState(std::make_unique<DeathScreenState>(game, world, 1));
    }
}

void MainGameState::render(sf::RenderWindow& window)
{
    // DRAWING SKY
    sf::RectangleShape sky({static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)});
    sky.setPosition({0.0f, 0.0f});
    sky.setFillColor(world.getSkyColor(world.getDayTime() / World::DAY_CYCLE_DURATION));
    window.draw(sky);


    auto& entities = world.getEntities();

    unsigned int unit_size = window.getSize().y / 9;

    sf::View view(
    {
        (entityWithID(1, world).getComponent<TransformComponent>().position.x + 0.5f) * unit_size,
        (entityWithID(1, world).getComponent<TransformComponent>().position.y - 0.5f) * unit_size
    },
    {
        (float)window.getSize().x,
        (float)window.getSize().y
    });

    view.setSize({view.getSize().x, -view.getSize().y});

    window.setView(view);
    
    RenderWorld(world, window);

    RenderSystem(entities, window);

    RenderBlockOverlay(world, window);

    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));

    healthBar.render(window);

    hotbar.render(window);

    if(inventoryWidget.isActive())
    {
        inventoryWidget.render(window);
    }
}