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
#include "../include/ExplosiveSystem.hpp"
#include "../include/HealthSystem.hpp"
#include "../include/TransformSystem.hpp"
#include "../include/ChunkUnloadSystem.hpp"

#include <iostream>

Entity& entityWithID(uint32_t id, World& world)
{
    for(auto& a : world.getEntities())
    {
        if(id == a.getID()) return a;
    }
    throw std::runtime_error("Entity with ID " + std::to_string(id) + " does not exist(entityWithID(int, World&))");
}

MainGameState::MainGameState(Game* game, World world) : GameState(game)
{
    this->world = std::move(world);  

    if(this->world.getEntities().empty())
    {
        std::cerr << "No entities!" << std::endl;
        throw std::runtime_error("World has no entities - cannot initialize MainGameState");
    }

    healthBar = HealthBar(UIElement::ScreenRelative{{0.05f, 0.05f}, {0.2f, 0.05f}, true, UIElement::ScreenRelative::Axis::Y});
    healthBar.updateScreenRelative(game->getWindow().getSize());
    
    try
    {
        inventoryWidget = InventoryWidget(&entityWithID(this->world.getPlayerID(), this->world).getComponent<InventoryComponent>());
        inventoryWidget.updateScreenRelative(game->getWindow().getSize());
        
        hotbar = Hotbar(&entityWithID(this->world.getPlayerID(), this->world).getComponent<InventoryComponent>());
        hotbar.updateScreenRelative(game->getWindow().getSize());
    } catch(const std::exception& e)
    {
        std::cerr << "Failed to initialize inventory widgets: " << e.what() << std::endl;
        throw;
    }
}

MainGameState::~MainGameState()
{
    world.save();
    std::cout << "MainGameState killed and world saved!" << std::endl;
}

void MainGameState::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::MouseButtonPressed>())
    {
        auto mouse = event.getIf<sf::Event::MouseButtonPressed>();

        if(mouse->button == sf::Mouse::Button::Left)
        {
            float unit_size = game->getWindow().getView().getSize().y / static_cast<float>(MainGameState::UNIT_SIZE_FACTOR);

            sf::View view(
            {
                (entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.x + 0.5f) * unit_size,
                (entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.y - 0.5f) * unit_size
            },
            {
                (float)game->getWindow().getSize().x,
                (float)game->getWindow().getSize().y
            });

            view.setSize({view.getSize().x, -view.getSize().y});            

            game->getWindow().setView(view);

            sf::Vector2i blockPos = getMouseBlockPosition(world, game->getWindow());

            if(world.getBlock(blockPos.x, blockPos.y).id != BlockID::Air && blockDatabase[world.getBlock(blockPos.x, blockPos.y).id].breakable && isBlockInRange(entityWithID(1, world).getComponent<TransformComponent>(), blockPos, 4.0f))
            {
                Entity newEntity(world.getPossibleID());
                newEntity.addComponent(TransformComponent{{blockPos.x + 0.25f, blockPos.y - 0.25f}, {0.5f, 0.5f}, sf::degrees(0.0f)});
                newEntity.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, false, false, false, true});
                newEntity.addComponent(ItemComponent{{static_cast<ItemID>(world.getBlock(blockPos.x, blockPos.y).id), 1}});
                newEntity.addComponent(RenderComponent{static_cast<unsigned short>(itemDatabase[newEntity.getComponent<ItemComponent>().item.itemID].texture), {{0, 0}, {16, 16}}, {0.5f, 0.5f}});
                world.setBlock(blockPos.x, blockPos.y, {BlockID::Air, 0});

                world.getEntities().push_back(std::move(newEntity));
            }
        }
        else if(mouse->button == sf::Mouse::Button::Right)
        {
            float unit_size = game->getWindow().getView().getSize().y / static_cast<float>(MainGameState::UNIT_SIZE_FACTOR);

            sf::View view(
            {
                (entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.x + 0.5f) * unit_size,
                (entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.y - 0.5f) * unit_size
            },
            {
                (float)game->getWindow().getSize().x,
                (float)game->getWindow().getSize().y
            });

            view.setSize({view.getSize().x, -view.getSize().y});

            game->getWindow().setView(view);

            sf::Vector2i blockPos = getMouseBlockPosition(world, game->getWindow());

            auto& inventory = entityWithID(world.getPlayerID(), world).getComponent<InventoryComponent>();

            if((world.getBlock(blockPos.x, blockPos.y).id == BlockID::Air || world.getBlock(blockPos.x, blockPos.y).id == BlockID::Water) && isBlockInRange(entityWithID(1, world).getComponent<TransformComponent>(), blockPos, 4.0f) && itemDatabase[inventory.inventory.slots[hotbar.getSelectedSlot()].itemID].category == ItemCategory::Block && inventory.inventory.slots[hotbar.getSelectedSlot()].empty() == false)
            {
                inventory.inventory.slots[hotbar.getSelectedSlot()].quantity--;

                world.setBlock(blockPos.x, blockPos.y, {itemToBlock(entityWithID(world.getPlayerID(), world).getComponent<InventoryComponent>().inventory.slots[hotbar.getSelectedSlot()].itemID), 0});
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
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) && entityWithID(world.getPlayerID(), world).getComponent<PhysicsComponent>().onGround)
    {
        auto& component = entityWithID(world.getPlayerID(), world).getComponent<PhysicsComponent>();
        component.velocity.y += 10.0f;
    }
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
    {
        auto& component = entityWithID(world.getPlayerID(), world).getComponent<PhysicsComponent>();
        component.force.x -= 45.0f;

        auto& render = entityWithID(world.getPlayerID(), world).getComponent<RenderComponent>();
        render.uv = {{0, 32}, {16, 16}};
    }
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
    {
        auto& component = entityWithID(world.getPlayerID(), world).getComponent<PhysicsComponent>();
        component.force.x += 45.0f;

        auto& render = entityWithID(world.getPlayerID(), world).getComponent<RenderComponent>();
        render.uv = {{32, 32}, {16, 16}};
    }
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape))
    {
        game->pushState(std::make_unique<PauseScreenState>(game));
    }

    TransformSystem(world);
    ExplosiveSystem(world, dt);
    HealthSystem(world);
    PhysicsSystem(entities, world, dt);
    InventorySystem(entities);
    ChunkUnloadSystem(world);

    healthBar.setHealth(entityWithID(world.getPlayerID(), world).getComponent<HealthComponent>());

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

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q))
    {
        bool full_stack = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift);

        auto& inventory = entityWithID(world.getPlayerID(), world).getComponent<InventoryComponent>().inventory;

        ItemStack& stack = inventory.slots[hotbar.getSelectedSlot()];

        if(stack.empty()) return;

        float unit_size = game->getWindow().getSize().y / 9.0f;

        sf::View view(
        {
            (entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.x + 0.5f) * unit_size,
            (entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.y - 0.5f) * unit_size
        },
        {
            (float)game->getWindow().getSize().x,
            (float)game->getWindow().getSize().y
        });

        view.setSize({view.getSize().x, -view.getSize().y});

        game->getWindow().setView(view);

        Entity item(world.getPossibleID());
        item.addComponent(TransformComponent{entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position + sf::Vector2f(0.0f, 1.5f), {0.5f, 0.5f}, sf::degrees(0.0f)});
        item.addComponent(PhysicsComponent{getMouseWorldPosition(world, game->getWindow()) - entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, false, false, false, true});
        item.addComponent(RenderComponent{static_cast<unsigned short>(itemDatabase[stack.itemID].texture), {{0, 0}, {16, 16}}, {0.5f, 0.5f}});

        if(full_stack)
        {            
            item.addComponent(ItemComponent{stack});

            stack = {ItemID::None, 0};
        }
        else
        {
            item.addComponent(ItemComponent{{stack.itemID, 1}});

            if(--stack.quantity == 0)
            {
                stack.itemID = ItemID::None;
            }
        }

        world.getEntities().push_back(std::move(item));
    }

    if(InputManager::isLazyKeyPressed(sf::Keyboard::Key::F3))
    {
        debug = !debug;
    }

    for(auto i = sf::Keyboard::Key::Num1; i <= sf::Keyboard::Key::Num9; i = static_cast<sf::Keyboard::Key>(static_cast<int>(i) + 1))
    {
        if(InputManager::isLazyKeyPressed(i))
        {
            hotbar.setSelectedSlot(static_cast<uint8_t>(i) - static_cast<uint8_t>(sf::Keyboard::Key::Num1));
        }
    }

    world.tick(dt);

    if(entityWithID(world.getPlayerID(), world).getComponent<HealthComponent>().health <= 0)
    {
        game->pushState(std::make_unique<DeathScreenState>(game, world, 1));
    }

    last_fps_update += dt;

    if(last_fps_update >= 1.0f)
    {
        last_fps_update -= 1.0f;

        fps = 1.0f / dt;
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

    unsigned int unit_size = window.getSize().y / UNIT_SIZE_FACTOR;

    sf::View view(
    {
        (entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.x + 0.5f) * unit_size,
        (entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.y - 0.5f) * unit_size
    },
    {
        (float)window.getSize().x,
        (float)window.getSize().y
    });



    view.setSize({view.getSize().x, -view.getSize().y});

    window.setView(view);

    RenderSystem(entities, window);

    RenderWorld(world, window); 
    
    RenderBlockOverlay(world, window);

    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));

    healthBar.render(window);

    hotbar.render(window);

    if(inventoryWidget.isActive())
    {
        inventoryWidget.render(window);
    }


    if(debug)
    {
        std::string debug_string = 
        "FPS: " + std::to_string(fps) + '\n' +
        "X: " + std::to_string(entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.x) +
        " Y: " + std::to_string(entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.y) + '\n' +
        "CHUNKS LOADED: " + std::to_string(world.getChunks().size()) + '\n';

        sf::Text debug_text(AssetManager::getFont(0), debug_string, 20);

        debug_text.setPosition({50.0f, 50.0f});

        debug_text.setFillColor(sf::Color::White);

        debug_text.setOutlineThickness(2.0f);

        debug_text.setOutlineColor(sf::Color::Black);

        window.draw(debug_text);
    }
}


bool isInRange(TransformComponent& player, TransformComponent& target, float range)
{
    sf::Vector2f player_closest = player.position + sf::Vector2f((target.position.x > player.position.x) ? player.size.x : 0.0f, (target.position.y > player.position.y) ? player.size.y : 0.0f);
    sf::Vector2f target_closest = target.position + sf::Vector2f((player.position.x > target.position.x) ? target.size.x : 0.0f, (player.position.y > target.position.y) ? target.size.y : 0.0f);

    float distance = std::sqrt(std::pow(player_closest.x - target_closest.x, 2) + std::pow(player_closest.y - target_closest.y, 2));

    return distance <= range;
}

bool isBlockInRange(TransformComponent& player, sf::Vector2i& block, float range)
{
    sf::Vector2f player_closest = player.position + sf::Vector2f((block.x > player.position.x) ? player.size.x : 0.0f, (block.y > player.position.y) ? player.size.y : 0.0f);
    sf::Vector2f block_closest = sf::Vector2f(block) + sf::Vector2f((player.position.x > static_cast<float>(block.x)) ? 1.0f : 0.0f, (player.position.y > static_cast<float>(block.y)) ? 1.0f : 0.0f);
    float distance = std::sqrt(std::pow(player_closest.x - block_closest.x, 2) + std::pow(player_closest.y - block_closest.y, 2));

    return distance <= range;
}