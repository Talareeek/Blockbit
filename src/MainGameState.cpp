#include "../include/MainGameState.hpp"
#include "../include/Game.hpp"
#include "../include/Entity.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/PhysicsSystem.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/AssetManager.hpp"
#include "../include/World.hpp"
#include "../include/PauseScreenState.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/InventoryWidget.hpp"
#include "../include/InputManager.hpp"
#include "../include/DeathScreenState.hpp"
#include "../include/AnimationSystem.hpp"
#include "../include/Render.hpp"
#include "../include/RenderSystem.hpp"

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

    tryInitializePlayerUI();
}

MainGameState::~MainGameState()
{
    game->getWindow().setTitle("Blockbit");

    if (saveOnDestruct)
    {
        try { world.save(); }
        catch (const std::exception& e)
        {
            std::cerr << "[MainGameState] world.save() failed: " << e.what() << '\n';
        }
    }

    game->getConsole().assignWorld(nullptr);
}

bool MainGameState::hasPlayerEntity() const
{
    if (!localPlayerEntityId.has_value()) return false;
    uint32_t pid = localPlayerEntityId.value();
    for (const auto& e : world.getEntities())
        if (e.getID() == pid) return true;
    return false;
}

void MainGameState::tryInitializePlayerUI()
{
    if (playerUIInitialized) return;
    if (!hasPlayerEntity()) return;

    auto& playerEntity = entityWithID(localPlayerEntityId.value(), world);

    if (!playerEntity.hasComponent<TransformComponent>()) return;

    if (playerEntity.hasComponent<HealthComponent>())
    {
        healthBar = HealthBar(&playerEntity.getComponent<HealthComponent>());
        healthBar.updateScreenRelative(game->getWindow().getSize());
    }

    if (playerEntity.hasComponent<InventoryComponent>())
    {
        inventoryWidget = InventoryWidget(&playerEntity.getComponent<InventoryComponent>());
        inventoryWidget.updateScreenRelative(game->getWindow().getSize());

        hotbar = Hotbar(&playerEntity.getComponent<InventoryComponent>());
        hotbar.updateScreenRelative(game->getWindow().getSize());
    }

    playerUIInitialized = true;
}

void MainGameState::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::Resized>())
    {
        world.chunkMeshes.clear();
    }

    if (!playerUIInitialized) return;

    if(event.is<sf::Event::MouseButtonPressed>())
    {
        float unit_size = game->getWindow().getView().getSize().y / static_cast<float>(MainGameState::UNIT_SIZE_FACTOR);

        sf::View view(
        {
            static_cast<float>((entityWithID(localPlayerEntityId.value(), world).getComponent<TransformComponent>().position.x + 0.5f) * unit_size),
            static_cast<float>((entityWithID(localPlayerEntityId.value(), world).getComponent<TransformComponent>().position.y - 0.5f) * unit_size)
        },
        {
            (float)game->getWindow().getSize().x,
            (float)game->getWindow().getSize().y
        });

        view.setSize({view.getSize().x, -view.getSize().y});

        game->getWindow().setView(view);
    }

    healthBar.handleEvent(event);
    inventoryWidget.handleEvent(event);
    hotbar.handleEvent(event);
}

void MainGameState::update(float dt)
{
    tryInitializePlayerUI();

    float tick_step = 1.0f / static_cast<float>(MainGameState::TICKS_PER_SECOND);

    if (dt > tick_step * 4.0f) dt = tick_step;

    since_last_tick += dt;

    int max_ticks_per_frame = 4;
    while(since_last_tick >= tick_step && max_ticks_per_frame-- > 0)
    {
        if (playerUIInitialized)
        {
            float unit_size = game->getWindow().getView().getSize().y / static_cast<float>(MainGameState::UNIT_SIZE_FACTOR);

            sf::View view(
            {
                static_cast<float>((entityWithID(localPlayerEntityId.value(), world).getComponent<TransformComponent>().position.x + 0.5f) * unit_size),
                static_cast<float>((entityWithID(localPlayerEntityId.value(), world).getComponent<TransformComponent>().position.y - 0.5f) * unit_size)
            },
            {
                (float)game->getWindow().getSize().x,
                (float)game->getWindow().getSize().y
            });

            view.setSize({view.getSize().x, -view.getSize().y});

            game->getWindow().setView(view);
        }

        onTick(tick_step);

        since_last_tick -= tick_step;
    }

    if (since_last_tick > tick_step) since_last_tick = 0.0f;

    AnimationSystem(world, dt);

    game->getConsole().assignWorld(&world);

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape))
    {
        game->pushState(std::make_unique<PauseScreenState>(game));
    }

    if (playerUIInitialized)
    {
        healthBar.setHealth(&entityWithID(localPlayerEntityId.value(), world).getComponent<HealthComponent>());

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
    }

    if(InputManager::isLazyKeyPressed(sf::Keyboard::Key::F3))
    {
        debug = !debug;
    }

    if (playerUIInitialized
        && entityWithID(localPlayerEntityId.value(), world).getComponent<HealthComponent>().health <= 0)
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
    auto [skyTop, skyBottom] = world.getSkyGradient(world.getDayTime() / World::DAY_CYCLE_DURATION);
    renderSky(window, skyTop, skyBottom);

    renderSunAndMoon(world.getDayTime(), window);

    if (!playerUIInitialized)
    {
        window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));
        return;
    }

    sf::Vector2<double> camera = entityWithID(localPlayerEntityId.value(), world).getComponent<TransformComponent>().position + sf::Vector2<double>(0.5, -0.5);

    RenderEntities(world, camera, window);

    RenderWorld(world, camera, window);

    RenderBlockOverlay(world, camera, window, localPlayerEntityId.value());

    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));

    if (hideUI) return;

    healthBar.render(window);
    hotbar.render(window);

    if(inventoryWidget.isActive())
    {
        inventoryWidget.render(window);
    }

    if(debug)
    {
        sf::Text debug_text(AssetManager::getFont(0), debugString(), 20);
        debug_text.setPosition({50.0f, 50.0f});
        debug_text.setFillColor(sf::Color::White);
        debug_text.setOutlineThickness(2.0f);
        debug_text.setOutlineColor(sf::Color::Black);
        window.draw(debug_text);
    }
}

std::string MainGameState::debugString()
{
    std::string debug_string = "FPS: " + std::to_string(fps) + '\n';

    if (playerUIInitialized)
    {
        auto simulationRange = world.getSimulationRangeForEntity(localPlayerEntityId.value());
        debug_string +=
            "X: " + std::to_string(entityWithID(localPlayerEntityId.value(), world).getComponent<TransformComponent>().position.x) +
            " Y: " + std::to_string(entityWithID(localPlayerEntityId.value(), world).getComponent<TransformComponent>().position.y) + '\n' +
            "CHUNKS LOADED: " + std::to_string(world.getChunks().size()) + '\n' +
            "MUSIC: " + std::to_string(music_timer) + " / " + std::to_string(music_interval) + '\n' +
            "SIMULATION RANGE: " + std::to_string(simulationRange.first) + " - " + std::to_string(simulationRange.second) + '\n' +
            "INPUTS: " + std::to_string(inputs.size()) + '\n';
    }

    return debug_string;
}

void processWorldInputs(World& world, std::vector<Input> inputs, uint32_t id)
{
    auto& entity = entityWithID(id, world);

    if(!entity.hasComponent<PhysicsComponent>() || !entity.hasComponent<RenderComponent>() || !entity.hasComponent<TransformComponent>() || !entity.hasComponent<InventoryComponent>()) return;


    auto& physics = entity.getComponent<PhysicsComponent>();
    auto& render = entity.getComponent<RenderComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& inventory = entity.getComponent<InventoryComponent>();

    for(const auto& input : inputs)
    {
        switch(input.type)
        {
            case InputType::MOVE:
            {
                auto direction = std::get<sf::Vector2f>(input.value);

                int bx = (int)std::floor(transform.position.x + transform.size.x / 2.0f);
                int by = (int)std::floor(transform.position.y);
                physics.force.x += 45.0f * direction.x / blockDatabase[world.getBlock(bx, by).id].drag;

                if(direction.x < 0.0f)
                {
                    render.uv = {{0, 32}, {16, 16}};
                }
                else if(direction.x > 0.0f)
                {
                    render.uv = {{32, 32}, {16, 16}};
                }

                break;
            }
            case InputType::JUMP:
            {
                if(physics.onGround)
                {
                    physics.velocity.y += 10.0f;
                }
                else if(isSubmerged(world, transform))
                {
                    physics.force.y += 60.0f;
                }

                break;
            }
            case InputType::ATTACK:
            {
                auto worldPos = std::get<sf::Vector2f>(input.value);
                sf::Vector2i blockPos = {static_cast<int>(std::floor(worldPos.x)), static_cast<int>(std::floor(worldPos.y))};

                if(world.getBlock(blockPos.x, blockPos.y).id != BlockID::Air && blockDatabase[world.getBlock(blockPos.x, blockPos.y).id].breakable && isBlockInRange(transform, blockPos, 4.0f))
                {
                    Entity newEntity(world.getPossibleID());
                    newEntity.addComponent(TransformComponent{{blockPos.x + 0.25f, blockPos.y - 0.25f}, {0.5f, 0.5f}, sf::degrees(0.0f)});
                    newEntity.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, false, false, false, true});
                    newEntity.addComponent(ItemComponent{{blockToItem(world.getBlock(blockPos.x, blockPos.y).id), 1}});
                    newEntity.addComponent(RenderComponent{static_cast<unsigned short>(itemDatabase[newEntity.getComponent<ItemComponent>().item.itemID].texture), {{0, 0}, {16, 16}}, {0.5f, 0.5f}});
                    world.setBlock(blockPos.x, blockPos.y, {BlockID::Air, 0});

                    world.getEntities().push_back(std::move(newEntity));
                }

                break;
            }
            case InputType::USE:
            {
                auto worldPos = std::get<sf::Vector2f>(input.value);
                sf::Vector2i blockPos = {static_cast<int>(std::floor(worldPos.x)), static_cast<int>(std::floor(worldPos.y))};

                auto& selected = inventory.inventory.slots[inventory.selectedSlot];

                if(selected.empty()) break;

                if((world.getBlock(blockPos.x, blockPos.y).id == BlockID::Air || world.getBlock(blockPos.x, blockPos.y).id == BlockID::Water) && isBlockInRange(transform, blockPos, 4.0f) && itemDatabase[selected.itemID].category == ItemCategory::Block)
                {
                    selected.quantity--;

                    world.setBlock(blockPos.x, blockPos.y, {itemToBlock(selected.itemID), 0});
                }
                else if(itemDatabase[selected.itemID].category != ItemCategory::Block)
                {
                    if(itemDatabase[selected.itemID].onUse(world, worldPos, id))
                    {
                        selected.quantity--;
                    }
                }

                break;
            }
            case InputType::DROP:
            {
                auto& info = std::get<DropInfo>(input.value);
                auto& stack = inventory.inventory.slots[inventory.selectedSlot];

                if(stack.empty()) break;

                Entity item(world.getPossibleID());
                item.addComponent(TransformComponent{transform.position + sf::Vector2<double>(0.0, 1.5), {0.5, 0.5}, sf::degrees(0.0f)});
                item.addComponent(PhysicsComponent{info.mousePosition - sf::Vector2f(transform.position), {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, false, false, false, true});
                item.addComponent(RenderComponent{static_cast<unsigned short>(itemDatabase[stack.itemID].texture), {{0, 0}, {16, 16}}, {0.5f, 0.5f}});

                if(info.fullStack)
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

                break;
            }
            case InputType::CHANGE_SLOT:
            {
                uint8_t slot = std::get<uint8_t>(input.value);
                inventory.selectedSlot = slot % 9;

                break;
            }
        }
    }
}

void MainGameState::processInputs(std::vector<Input> inputs, uint32_t id)
{
    processWorldInputs(world, std::move(inputs), id);
}

bool isInRange(TransformComponent& player, TransformComponent& target, float range)
{
    sf::Vector2f player_closest = sf::Vector2f(player.position) + sf::Vector2f((target.position.x > player.position.x) ? static_cast<float>(player.size.x) : 0.0f, (target.position.y > player.position.y) ? static_cast<float>(player.size.y) : 0.0f);
    sf::Vector2f target_closest = sf::Vector2f(target.position) + sf::Vector2f((player.position.x > target.position.x) ? static_cast<float>(target.size.x) : 0.0f, (player.position.y > target.position.y) ? static_cast<float>(target.size.y) : 0.0f);

    float distance = std::sqrt(std::pow(player_closest.x - target_closest.x, 2) + std::pow(player_closest.y - target_closest.y, 2));

    return distance <= range;
}

bool isBlockInRange(TransformComponent& player, sf::Vector2i& block, float range)
{
    sf::Vector2f player_closest = sf::Vector2f(player.position) + sf::Vector2f((block.x > player.position.x) ? static_cast<float>(player.size.x) : 0.0f, (block.y > player.position.y) ? static_cast<float>(player.size.y) : 0.0f);
    sf::Vector2f block_closest = sf::Vector2f(block) + sf::Vector2f((player.position.x > static_cast<float>(block.x)) ? 1.0f : 0.0f, (player.position.y > static_cast<float>(block.y)) ? 1.0f : 0.0f);
    float distance = std::sqrt(std::pow(player_closest.x - block_closest.x, 2) + std::pow(player_closest.y - block_closest.y, 2));

    return distance <= range;
}
