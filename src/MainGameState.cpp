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
#include "../include/AISystem.hpp"
#include "../include/AnimationSystem.hpp"
#include "../include/Render.hpp"

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
    game->getWindow().setTitle("Blockbit - Singleplayer");

    this->world = std::move(world);  

    if(this->world.getEntities().empty())
    {
        std::cerr << "No entities!" << std::endl;
        throw std::runtime_error("World has no entities - cannot initialize MainGameState");
    }

    healthBar = HealthBar(&entityWithID(this->world.getPlayerID(), this->world).getComponent<HealthComponent>());
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
    game->getWindow().setTitle("Blockbit");

    world.save();
    game->getConsole().assignWorld(nullptr);
}

void MainGameState::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::MouseButtonPressed>())
    {
        float unit_size = game->getWindow().getView().getSize().y / static_cast<float>(MainGameState::UNIT_SIZE_FACTOR);

        sf::View view(
        {
            static_cast<float>((entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.x + 0.5f) * unit_size),
            static_cast<float>((entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.y - 0.5f) * unit_size)
        },
        {
            (float)game->getWindow().getSize().x,
            (float)game->getWindow().getSize().y
        });

        view.setSize({view.getSize().x, -view.getSize().y});

        game->getWindow().setView(view);
    }

    if(event.is<sf::Event::Resized>())
    {
        world.chunkMeshes.clear();
    }

    healthBar.handleEvent(event);

    inventoryWidget.handleEvent(event);

    hotbar.handleEvent(event);

    auto new_inputs = getInputsFromEvent(event);

    inputs.insert(inputs.end(), std::make_move_iterator(new_inputs.begin()), std::make_move_iterator(new_inputs.end()));
}

void MainGameState::update(float dt)
{
    float tick_step = 1.0f / static_cast<float>(MainGameState::TICKS_PER_SECOND);

    since_last_tick += dt;

    auto& entities = world.getEntities();

    // DEPENDANT BY TICK-RATE
    while(since_last_tick >= tick_step)
    {
        float unit_size = game->getWindow().getView().getSize().y / static_cast<float>(MainGameState::UNIT_SIZE_FACTOR);

        sf::View view(
        {
            static_cast<float>((entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.x + 0.5f) * unit_size),
            static_cast<float>((entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.y - 0.5f) * unit_size)
        },
        {
            (float)game->getWindow().getSize().x,
            (float)game->getWindow().getSize().y
        });

        view.setSize({view.getSize().x, -view.getSize().y});

        game->getWindow().setView(view);

        auto new_inputs = getInputs();
        inputs.insert(inputs.end(), std::make_move_iterator(new_inputs.begin()), std::make_move_iterator(new_inputs.end()));

        processInputs(std::move(inputs), world.getPlayerID());
        inputs.clear();

        AISystem(world, tick_step);
        TransformSystem(world);
        ExplosiveSystem(world, tick_step);
        HealthSystem(world);
        PhysicsSystem(entities, world, tick_step);
        InventorySystem(entities);
        ChunkUnloadSystem(world);

        since_last_tick -= tick_step;
    }

    AnimationSystem(world, dt);

    game->getConsole().assignWorld(&world);
    
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape))
    {
        game->pushState(std::make_unique<PauseScreenState>(game));
    }    

    healthBar.setHealth(&entityWithID(world.getPlayerID(), world).getComponent<HealthComponent>());

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

    if(InputManager::isLazyKeyPressed(sf::Keyboard::Key::F3))
    {
        debug = !debug;
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

    bool music_playing;

    for(auto& music : AssetManager::musics)
    {
        if(music.second.getStatus() == sf::Music::Status::Playing)
        {
            music_playing = true;
            break;
        }
    }

    /*
    if(!music_playing)
    {
        music_timer += dt;

        if(music_timer >= music_interval)
        {
            music_timer = 0.0f;
            music_interval = std::rand() % 420 + 180.0f;

            AssetManager::getMusic(static_cast<AssetManager::MusicID>(std::rand() % AssetManager::musics.size())).play();
        }
    }
    */
}

void MainGameState::render(sf::RenderWindow& window)
{
    // DRAWING SKY
    sf::RectangleShape sky({static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)});
    sky.setPosition({0.0f, 0.0f});
    sky.setFillColor(world.getSkyColor(world.getDayTime() / World::DAY_CYCLE_DURATION));
    window.draw(sky);

    renderSunAndMoon(world.getDayTime(), window);

    auto& entities = world.getEntities();

    unsigned int unit_size = window.getSize().y / UNIT_SIZE_FACTOR;

    sf::View view(
    {
        static_cast<float>((entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.x + 0.5f) * unit_size),
        static_cast<float>((entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.y - 0.5f) * unit_size)
    },
    {
        (float)window.getSize().x,
        (float)window.getSize().y
    });



    view.setSize({view.getSize().x, -view.getSize().y});

    window.setView(view);

    RenderSystem(entities, window);

    RenderWorld(world, window);

    //RenderLightRays(world, window);

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
    auto simulationRange = world.getSimulationRangeForEntity(world.getPlayerID());

    std::string debug_string = 
    "FPS: " + std::to_string(fps) + '\n' +
    "X: " + std::to_string(entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.x) +
    " Y: " + std::to_string(entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>().position.y) + '\n' +
    "CHUNKS LOADED: " + std::to_string(world.getChunks().size()) + '\n' +
    "MUSIC: " + std::to_string(music_timer) + " / " + std::to_string(music_interval) + '\n' +
    "SIMULATION RANGE: " + std::to_string(simulationRange.first) + " - " + std::to_string(simulationRange.second) + '\n' +
    "INPUTS: " + std::to_string(inputs.size()) + '\n';

    return debug_string;
}

std::vector<Input> MainGameState::getInputs()
{
    std::vector<Input> inputs;

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
    {
        inputs.push_back(Input{InputType::MOVE, sf::Vector2f{-1.0f, 0.0f}});
    }

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
    {
        inputs.push_back(Input{InputType::MOVE, sf::Vector2f{1.0f, 0.0f}});
    }

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
    {
        inputs.push_back({InputType::JUMP, std::monostate{}});
    }

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q))
    {
        inputs.push_back({InputType::DROP, DropInfo{getMouseWorldPosition(world, game->getWindow()), sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)}});
    }

    return inputs;
}

std::vector<Input> MainGameState::getInputsFromEvent(const sf::Event& event)
{
    std::vector<Input> inputs;

    if(event.is<sf::Event::MouseButtonPressed>())
    {
        auto mouse = event.getIf<sf::Event::MouseButtonPressed>();

        if(mouse->button == sf::Mouse::Button::Left)
        {
            inputs.push_back({InputType::ATTACK, getMouseWorldPosition(world, game->getWindow())});
        }
        else if(mouse->button == sf::Mouse::Button::Right)
        {
            inputs.push_back({InputType::USE, getMouseWorldPosition(world, game->getWindow())});
        }
    }

    else if(event.is<sf::Event::MouseWheelScrolled>())
    {
        auto& inventory = entityWithID(world.getPlayerID(), world).getComponent<InventoryComponent>();
        inputs.push_back({InputType::CHANGE_SLOT, inventory.selectedSlot});
    }

    else if(event.is<sf::Event::KeyPressed>())
    {
        auto key = event.getIf<sf::Event::KeyPressed>();

        if(key->code >= sf::Keyboard::Key::Num1 && key->code <= sf::Keyboard::Key::Num9)
        {
            uint8_t slot = static_cast<uint8_t>(static_cast<int>(key->code) - static_cast<int>(sf::Keyboard::Key::Num1));
            inputs.push_back({InputType::CHANGE_SLOT, slot});
        }
    }

    return inputs;
}


void MainGameState::processInputs(std::vector<Input> inputs, uint32_t id)
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

                physics.force.x += 45.0f * direction.x;

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

                auto& selected = inventory.inventory.slots[hotbar.getSelectedSlot()];

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
                auto& stack = inventory.inventory.slots[hotbar.getSelectedSlot()];

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
                hotbar.setSelectedSlot(slot);

                break;
            }
        }
    }
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