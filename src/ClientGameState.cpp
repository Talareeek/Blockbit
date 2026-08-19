#include "../include/ClientGameState.hpp"

#include "../include/Game.hpp"
#include "../include/GameCommon.hpp"
#include "../include/Entity.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/AssetManager.hpp"
#include "../include/Chunk.hpp"
#include "../include/PauseScreenState.hpp"
#include "../include/DeathScreenState.hpp"
#include "../include/InputManager.hpp"
#include "../include/AnimationSystem.hpp"
#include "../include/Render.hpp"
#include "../include/RenderSystem.hpp"
#include "../include/NetworkClientTransport.hpp"
#include "../include/NetworkServerTransport.hpp"
#include "../include/LoopbackServerTransport.hpp"
#include "../include/CompositeServerTransport.hpp"
#include "../include/PhysicsSystem.hpp"
#include "../include/AnnouncementState.hpp"
#include "../include/World.hpp"
#include "../include/Climate.hpp"
#include "../include/NetworkInterpolationComponent.hpp"

#include <iostream>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdlib>

ClientGameState::ClientGameState(Game* game, std::filesystem::path world_path, uint16_t network_port, std::string nickname) : GameState(game)
{
    game->getWindow().setTitle("Blockbit - " + (network_port == 0) ? "Singleplayer" : "Host");

    this->nickname = std::move(nickname);

    auto loopback_pair = makeLoopbackPair();

    
    if(network_port == 0)
    {
        local_server.emplace(world_path, std::move(loopback_pair.serverSide));
    }
    else
    {
        auto composite = std::make_unique<CompositeServerTransport>();
        composite->add(std::move(loopback_pair.serverSide));
        composite->add(std::make_unique<NetworkServerTransport>(network_port));

        local_server.emplace(world_path, std::move(composite));
        remote_address = "0.0.0.0:" + std::to_string(network_port);
        std::cerr << "[Server] Listening on port " << network_port << '\n';
    }

    transport = std::move(loopback_pair.clientSide);
    transport->connect("loopback", 0);
    transport->send(serializePacket(LoginPacket{this->nickname}));
    login_sent = true;

    was_connected = true;
    connect_attempted = true;

    chat_ui.assignChat(&chat);
    chat_ui.setOnSend([this](std::wstring message)
    {
        if (!transport || !transport->isConnected()) return;
        transport->send(serializePacket(ChatMessagePacket{std::move(message)}));
    });
}

ClientGameState::ClientGameState(Game* game, std::string name, unsigned int seed, GenerationProperties generation_properties, uint16_t network_port, std::string nickname) : GameState(game)
{
    game->getWindow().setTitle("Blockbit - " + (network_port == 0) ? "Singleplayer" : "Host");

    this->nickname = std::move(nickname);

    auto loopback_pair = makeLoopbackPair();

    
    if(network_port == 0)
    {
        local_server.emplace(name, seed, generation_properties, std::move(loopback_pair.serverSide));
    }
    else
    {
        auto composite = std::make_unique<CompositeServerTransport>();
        composite->add(std::move(loopback_pair.serverSide));
        composite->add(std::make_unique<NetworkServerTransport>(network_port));

        local_server.emplace(name, seed, generation_properties, std::move(composite));
        remote_address = "0.0.0.0:" + std::to_string(network_port);
        std::cerr << "[Server] Listening on port " << network_port << '\n';
    }

    transport = std::move(loopback_pair.clientSide);
    transport->connect("loopback", 0);
    transport->send(serializePacket(LoginPacket{this->nickname}));
    login_sent = true;

    was_connected = true;
    connect_attempted = true;

    chat_ui.assignChat(&chat);
    chat_ui.setOnSend([this](std::wstring message)
    {
        if (!transport || !transport->isConnected()) return;
        transport->send(serializePacket(ChatMessagePacket{std::move(message)}));
    });
}

ClientGameState::ClientGameState(Game* game, const std::string& host, uint16_t port, std::string nickname) : GameState(game)
{
    game->getWindow().setTitle("Blockbit - Multiplayer");

    pending_host = host;
    pending_port = port;
    remote_address = host + ":" + std::to_string(port);
    this->nickname = std::move(nickname);

    transport = std::make_unique<NetworkClientTransport>();

    chat_ui.assignChat(&chat);
    chat_ui.setOnSend([this](std::wstring message)
    {
        if (!transport || !transport->isConnected()) return;
        transport->send(serializePacket(ChatMessagePacket{std::move(message)}));
    });

    game->getConsole().writeLine(L"[Client] Will attempt connection to " + std::wstring(remote_address.begin(), remote_address.end()));
}

ClientGameState::~ClientGameState()
{
    if (transport) transport->disconnect();

    game->getWindow().setTitle("Blockbit");
    game->getConsole().assignWorld(nullptr);
}

bool ClientGameState::hasPlayerEntity() const
{
    if (!local_player_entity_id.has_value()) return false;

    UUID player_id = local_player_entity_id.value();
    for (const auto& [id, entity] : local_world.getEntities())
        if (entity.getID() == player_id) return true;
    return false;
}

void ClientGameState::tryInitializePlayerUI()
{
    if (player_ui_initialized) return;
    if (!hasPlayerEntity()) return;

    auto& player_entity = local_world.getEntity(local_player_entity_id.value());

    if (!player_entity.hasComponent<TransformComponent>()) return;

    if (player_entity.hasComponent<HealthComponent>())
    {
        health_bar = HealthBar(&player_entity.getComponent<HealthComponent>());
        health_bar.updateScreenRelative(game->getWindow().getSize());
    }

    if (player_entity.hasComponent<InventoryComponent>())
    {
        inventory_widget = InventoryWidget(&player_entity.getComponent<InventoryComponent>());
        inventory_widget.updateScreenRelative(game->getWindow().getSize());

        hotbar = Hotbar(&player_entity.getComponent<InventoryComponent>());
        hotbar.updateScreenRelative(game->getWindow().getSize());
    }

    player_ui_initialized = true;
}

void ClientGameState::applySnapshot(const SnapshotPacket& snapshot)
{
    latest_tick = snapshot.tick;

    std::unordered_set<UUID> present_ids;
    present_ids.reserve(snapshot.entities.size());

    for (const NetEntity& net_entity : snapshot.entities)
    {
        present_ids.insert(net_entity.id);

        bool is_new = !local_world.doesEntityExist(net_entity.id);

        if (is_new)
        {
            Entity entity(net_entity.id);

            entity.addComponent(TransformComponent{{net_entity.x, net_entity.y}, {net_entity.size_x, net_entity.size_y}, sf::degrees(0.0f)});
            entity.addComponent(RenderComponent{static_cast<uint16_t>(net_entity.textureID), sf::IntRect{{net_entity.uv_x, net_entity.uv_y}, {net_entity.uv_size_x, net_entity.uv_size_y}}, {net_entity.size_x, net_entity.size_y}});
            entity.addComponent(HealthComponent{net_entity.health, net_entity.maxHealth, false});

            if (!net_entity.inventory.empty())
            {
                InventoryComponent inventory_component(net_entity.inventory.size());

                for (size_t i = 0; i < net_entity.inventory.size(); i++)
                {
                    inventory_component.inventory.slots[i].itemID = static_cast<ItemID>(net_entity.inventory[i].itemID);
                    inventory_component.inventory.slots[i].quantity = net_entity.inventory[i].quantity;
                }
                inventory_component.selectedSlot = net_entity.selectedSlot;

                entity.addComponent(std::move(inventory_component));
            }

            local_world.addEntity(std::move(entity));

            if (net_entity.id == local_player_entity_id.value())
            {
                player_ui_initialized = false;
            }

            continue;
        }

        auto& entity = local_world.getEntity(net_entity.id);

        entity.getComponent<TransformComponent>().teleport({net_entity.x, net_entity.y});

        auto& health = entity.getComponent<HealthComponent>();
        health.health = net_entity.health;
        health.maxHealth = net_entity.maxHealth;

        if (entity.hasComponent<InventoryComponent>() && !net_entity.inventory.empty())
        {
            auto& inventory = entity.getComponent<InventoryComponent>();

            for (size_t i = 0; i < net_entity.inventory.size() && i < inventory.inventory.slots.size(); i++)
            {
                inventory.inventory.slots[i].itemID = static_cast<ItemID>(net_entity.inventory[i].itemID);
                inventory.inventory.slots[i].quantity = net_entity.inventory[i].quantity;
            }
            inventory.selectedSlot = net_entity.selectedSlot;
        }

        if (net_entity.id != local_player_entity_id.value())
        {
            if (!entity.hasComponent<NetworkInterpolationComponent>()) entity.addComponent<NetworkInterpolationComponent>(NetworkInterpolationComponent{});

            entity.getComponent<NetworkInterpolationComponent>().pushSample(snapshot.tick, {net_entity.x, net_entity.y}, sf::Angle{});
        }
    }

    std::vector<UUID> to_remove;
    for (auto& [id, entity] : local_world.getEntities())
    {
        if (!present_ids.contains(id)) to_remove.push_back(id);
    }
    for (UUID id : to_remove) local_world.removeEntity(id);

    local_world.dayTime = snapshot.dayTime;
}

void ClientGameState::processIncoming()
{
    if (!transport) return;

    std::vector<ReceivedPacket> packets;
    try { packets = transport->poll(); }
    catch (const std::bad_alloc&) { error_message = "Out of memory"; connection_failed = true; return; }

    for (auto& packet : packets)
    {
        try
        {
            PacketReader reader(packet.payload.data(), packet.payload.size());

            switch (packet.type)
            {
                case PacketType::Initialization:
                {
                    auto init = deserializeInitialization(reader);

                    tick_rate = init.tick_rate;

                    break;
                }
                case PacketType::Chunk:
                {
                    auto initialization = deserializeChunk(reader);

                    auto& chunk = local_world.getChunks()[initialization.chunk.chunk_position];
                    chunk = initialization.chunk;

                    chunk.meshDirty = true;
                    chunkMeshes[initialization.chunk.chunk_position].built = false;
                    initialized = true;

                    break;
                }
                case PacketType::Snapshot:
                {
                    auto snapshot = deserializeSnapshot(reader);
                    applySnapshot(snapshot);

                    break;
                }
                case PacketType::BlockUpdate:
                {
                    auto block_update = deserializeBlockUpdate(reader);
                    local_world.setBlock(block_update.x, block_update.y, block_update.block);
                    break;
                }
                case PacketType::Spawn:
                {
                    auto spawn = deserializeSpawn(reader);
                    my_entity_id = spawn.id;
                    local_player_entity_id = my_entity_id;
                    break;
                }
                case PacketType::Despawn:
                {
                    auto despawn = deserializeDespawn(reader);
                    auto& entities = local_world.getEntities();

                    local_world.getEntities().erase(despawn.id);

                    break;
                }
                case PacketType::ChatMessage:
                {
                    auto message = deserializeChatMessage(reader);
                    chat.pushMessage(message.message);
                    break;
                }
                case PacketType::Input:
                    break;
                default:
                    break;
            }
        }
        catch (const std::bad_alloc&)
        {
            std::cerr << "[Client] bad_alloc decoding packet\n";
            error_message = "Out of memory";
            connection_failed = true;
            return;
        }
        catch (const std::exception& exception)
        {
            std::cerr << "[Client] Bad packet: " << exception.what() << '\n';
        }
    }
}

void ClientGameState::sendTickInputs()
{
    if (!transport || !transport->isConnected()) return;
    if (my_entity_id == UUID()) return;

    auto polled = getInputs(local_world, game->getWindow());
    inputs.insert(inputs.end(), std::make_move_iterator(polled.begin()), std::make_move_iterator(polled.end()));

    if (inputs.empty()) return;

    InputPacket packet;
    packet.id = my_entity_id;
    packet.inputs = std::move(inputs);
    inputs.clear();

    transport->send(serializePacket(packet));
}

void ClientGameState::onTick(float tick_step)
{
    if (acceptsPlayerInput())
    {
        if (transport && transport->isConnected())
        {
            sendTickInputs();
        }
    }
    else
    {
        inputs.clear();
    }

    if (local_server)
    {
        local_server->update(tick_step);
    }

    if (transport)
    {
        processIncoming();
    }
}

void ClientGameState::handleEvent(const sf::Event& event)
{
    bool chat_was_active = chat_ui.isActive();
    chat_ui.handleEvent(event);
    if (chat_was_active && !chat_ui.isActive())
    {
        chat_close_cooldown = 0.3f;
    }

    if (chat_ui.isActive()) return;

    if (auto key = event.getIf<sf::Event::KeyPressed>())
    {
        if (!chat_was_active && key->code == sf::Keyboard::Key::T)
        {
            chat_ui.open();
            return;
        }

        if (key->code == sf::Keyboard::Key::F1)
        {
            hide_ui = !hide_ui;
        }
        else if (key->code == sf::Keyboard::Key::F2)
        {
            pending_screenshot = true;
        }
    }

    if(event.is<sf::Event::Resized>())
    {
        chunkMeshes.clear();
    }

    if (player_ui_initialized)
    {
        if(event.is<sf::Event::MouseButtonPressed>())
        {
            float unit_size = game->getWindow().getView().getSize().y / static_cast<float>(WORLD_UNIT_SIZE_FACTOR);

            /*
            sf::View view{
                sf::Vector2f{
                    static_cast<float>(
                        local_world.getEntity(local_player_entity_id.value())
                            .getComponent<TransformComponent>()
                            .position.x + 0.5f
                    ) * unit_size,

                    static_cast<float>(
                        local_world.getEntity(local_player_entity_id.value())
                            .getComponent<TransformComponent>()
                            .position.y - 0.5f
                    ) * unit_size
                },
                sf::Vector2f{
                    static_cast<float>(game->getWindow().getSize().x),
                    static_cast<float>(game->getWindow().getSize().y)
                }
            };

            view.setSize({view.getSize().x, -view.getSize().y});

            game->getWindow().setView(view);
            */
        }

        health_bar.handleEvent(event);
        inventory_widget.handleEvent(event);
        hotbar.handleEvent(event);
    }

    uint8_t* slot_pointer = nullptr;
    if (hasPlayerEntity())
    {
        auto& player_entity = local_world.getEntity(local_player_entity_id.value());
        if (player_entity.hasComponent<InventoryComponent>())
        {
            slot_pointer = &player_entity.getComponent<InventoryComponent>().selectedSlot;
        }
    }
    if (!slot_pointer) slot_pointer = &local_selected_slot;

    sf::View game_view({0.0f, 0.0f}, {static_cast<float>(game->getWindow().getSize().x), static_cast<float>(game->getWindow().getSize().y)});
    game_view.setSize({game_view.getSize().x, -game_view.getSize().y});
    game->getWindow().setView(game_view);

    auto new_inputs = ::getInputsFromEvent(event, camera, game->getWindow(), *slot_pointer);
    inputs.insert(inputs.end(), std::make_move_iterator(new_inputs.begin()), std::make_move_iterator(new_inputs.end()));
}

void ClientGameState::update(float dt)
{
    if (connection_failed)
    {

        std::string title = was_connected ? "Disconnected" : "Cannot connect to server";
        std::string detail = remote_address.empty() ? "" : remote_address;
        std::string reason = error_message.empty() ? "" : ("Reason: " + error_message);
        std::string hint = "Press ESC to go back";

        Game* temp_game = game;
        game->popState(this);

        game->pushState(this, std::make_unique<AnnouncementState>(temp_game, title + '\n' + detail + '\n' + reason + '\n' + hint));

        return;
    }

    chat_ui.update(dt);
    if (chat_close_cooldown > 0.0f) chat_close_cooldown -= dt;

    if (!isLocalSession())
    {
        try
        {
            if (!connect_attempted && !connection_failed)
            {
                connect_attempted = true;
                if (!transport->connect(pending_host, pending_port))
                {
                    connection_failed = true;
                    error_message = transport->getLastError();
                    if (error_message.empty()) error_message = "Connection failed";
                    std::cerr << "[Client] connect failed: " << error_message << '\n';
                    return;
                }
                was_connected = true;
                std::cerr << "[Client] connected to " << remote_address << '\n';
            }

            if (was_connected && !login_sent && transport->isConnected())
            {
                transport->send(serializePacket(LoginPacket{nickname}));
                login_sent = true;
                std::cerr << "[Client] Sent Login as \"" << nickname << "\"\n";
            }

            if (!connection_failed && was_connected && !transport->isConnected())
            {
                connection_failed = true;
                if (error_message.empty())
                {
                    error_message = transport->getLastError();
                    if (error_message.empty()) error_message = "Disconnected from server";
                }
            }

            if (connection_failed) return;
        }
        catch (const std::bad_alloc&)
        {
            std::cerr << "[Client] bad_alloc in update\n";
            connection_failed = true;
            error_message = "Out of memory while updating";
            return;
        }
        catch (const std::exception& exception)
        {
            std::cerr << "[Client] exception in update: " << exception.what() << '\n';
            connection_failed = true;
            error_message = exception.what();
            return;
        }
    }

    tryInitializePlayerUI();

    float tick_step = 1.0f / static_cast<float>(WORLD_TICKS_PER_SECOND);

    if (dt > tick_step * 4.0f) dt = tick_step;

    since_last_tick += dt;

    int max_ticks_per_frame = 4;
    while(since_last_tick >= tick_step && max_ticks_per_frame-- > 0)
    {
        if (player_ui_initialized)
        {
            float unit_size = game->getWindow().getView().getSize().y / static_cast<float>(WORLD_UNIT_SIZE_FACTOR);

            sf::View view{
                sf::Vector2f{
                    static_cast<float>(
                        local_world.getEntity(local_player_entity_id.value())
                            .getComponent<TransformComponent>()
                            .position.x + 0.5f
                    ) * unit_size,

                    static_cast<float>(
                        local_world.getEntity(local_player_entity_id.value())
                            .getComponent<TransformComponent>()
                            .position.y - 0.5f
                    ) * unit_size
                },
                sf::Vector2f{
                    static_cast<float>(game->getWindow().getSize().x),
                    static_cast<float>(game->getWindow().getSize().y)
                }
            };

            view.setSize({view.getSize().x, -view.getSize().y});

            game->getWindow().setView(view);
        }

        onTick(tick_step);


        sf::View game_view({0.0f, 0.0f}, {static_cast<float>(game->getWindow().getSize().x), static_cast<float>(game->getWindow().getSize().y)});
        game_view.setSize({game_view.getSize().x, -game_view.getSize().y});
        game->getWindow().setView(game_view);

        ClientSnapshotPacket packet = {.cursor_x = getMouseWorldPosition(camera, game->getWindow()).x, .cursor_y = getMouseWorldPosition(camera, game->getWindow()).y};

        transport->send(serializePacket(packet));

        since_last_tick -= tick_step;
    }

    if (since_last_tick > tick_step) since_last_tick = 0.0f;

    PhysicsSystem(local_world, dt);

    AnimationSystem(local_world, dt);

    NetworkInterpolationSystem(local_world, latest_tick, getTickStep(tick_rate));

    game->getConsole().assignWorld(local_server.has_value() ? &local_server->getWorld() : &local_world);

    if(acceptsPlayerInput() && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape))
    {
        game->pushState(this, std::make_unique<PauseScreenState>(game));
    }

    if (player_ui_initialized)
    {
        health_bar.setHealth(&local_world.getEntity(local_player_entity_id.value()).getComponent<HealthComponent>());

        inventory_widget.updateScreenRelative(game->getWindow().getSize());
        hotbar.updateScreenRelative(game->getWindow().getSize());

        health_bar.update(dt);
        hotbar.update(dt);

        if(acceptsPlayerInput() && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E))
        {
            inventory_widget.setActive(!inventory_widget.isActive());
        }

        if(inventory_widget.isActive())
        {
            inventory_widget.update(dt);
        }
    }

    if(acceptsPlayerInput() && InputManager::isLazyKeyPressed(sf::Keyboard::Key::F3))
    {
        debug = !debug;
    }

    if (player_ui_initialized && local_world.getEntity(local_player_entity_id.value()).getComponent<HealthComponent>().health <= 0 && this->onTop())
    {
        World& death_world = local_server.has_value() ? local_server->getWorld() : local_world;
        game->pushState(this, std::make_unique<DeathScreenState>(game, death_world, local_player_entity_id.value()));
    }

    last_fps_update += dt;

    if(last_fps_update >= 1.0f)
    {
        last_fps_update -= 1.0f;

        fps = 1.0f / dt;
    }
}

void ClientGameState::render(sf::RenderWindow& window)
{
    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));

    if (!isLocalSession() && !initialized)
    {
        auto [sky_top, sky_bottom] = getSkyGradient(local_world.getDayTime() / World::DAY_CYCLE_DURATION);
        renderSky(window, sky_top, sky_bottom);

        sf::Text waiting(AssetManager::getFont(AssetManager::FontID::PressStart2P), "Connecting to " + remote_address + "...", 28);
        waiting.setFillColor(sf::Color::White);
        waiting.setOutlineColor(sf::Color::Black);
        waiting.setOutlineThickness(2.0f);
        auto waiting_bounds = waiting.getLocalBounds();
        waiting.setPosition({(window.getSize().x - waiting_bounds.size.x) * 0.5f, window.getSize().y * 0.5f});
        window.draw(waiting);
        return;
    }


    // GAME RENDER
    sf::View game_view({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)});
    game_view.setSize({game_view.getSize().x, -game_view.getSize().y});
    window.setView(game_view);

    // SKY
    auto [sky_top, sky_bottom] = getSkyGradient(local_world.getDayTime() / World::DAY_CYCLE_DURATION);
    renderSky(window, sky_top, sky_bottom);

    renderStars(local_world.getDayTime(), window);

    // SUN AND MOON
    renderSunAndMoon(local_world.getDayTime(), window);

    // UPDATING CAMERA
    if(hasPlayerEntity()) camera = local_world.getEntity(local_player_entity_id.value()).getComponent<TransformComponent>().position + sf::Vector2<double>(0.5, -0.5);

    // ENTITIES
    RenderEntities(local_world, camera, window);

    // WORLD
    RenderWorld(local_world, camera, window);

    // BLOCK OUTLINE
    RenderBlockOutline(camera, getMouseBlockPosition(camera, window), window);


    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));

    if (!hide_ui)
    {
        health_bar.render(window);
        hotbar.render(window);

        if(inventory_widget.isActive())
        {
            inventory_widget.render(window);
        }

        if(debug)
        {
            window.setView(sf::View({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)}));
            sf::Text debug_text(AssetManager::getFont(AssetManager::FontID::PressStart2P), debugString(), 20);
            window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));
            debug_text.setPosition({50.0f, 50.0f});
            debug_text.setFillColor(sf::Color::White);
            debug_text.setOutlineThickness(2.0f);
            debug_text.setOutlineColor(sf::Color::Black);
            window.draw(debug_text);
        }
    }

    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));
    chat_ui.render(window);

    if (pending_screenshot)
    {
        pending_screenshot = false;
        saveScreenshot(window);
    }
}

std::string ClientGameState::debugString()
{
    std::string debug_string = "FPS: " + std::to_string(fps) + '\n';

    if (player_ui_initialized)
    {
        auto simulation_range = local_world.getSimulationRangeForEntity(local_player_entity_id.value());
        Climate climate = local_world.climateAt(static_cast<int>(local_world.getEntity(local_player_entity_id.value()).getComponent<TransformComponent>().position.x));

        debug_string +=
            "X: " + std::to_string(local_world.getEntity(local_player_entity_id.value()).getComponent<TransformComponent>().position.x) +
            " Y: " + std::to_string(local_world.getEntity(local_player_entity_id.value()).getComponent<TransformComponent>().position.y) + '\n' +
            "CHUNKS LOADED: " + std::to_string(local_world.getChunks().size()) + '\n' +
            "SIMULATION RANGE: " + std::to_string(simulation_range.first) + " - " + std::to_string(simulation_range.second) + '\n' +
            "INPUTS: " + std::to_string(inputs.size()) + '\n' +
            "CLIMATE:\n" +
            "\tTEMPERATURE: " + std::to_string(climate.temperature) + '\n' +
            "\tHUMIDITY: " + std::to_string(climate.humidity) + '\n' +
            "\tCONTINENTALNESS: " + std::to_string(climate.continentalness) + '\n' +
            "\tEROSION: " + std::to_string(climate.erosion) + '\n' +
            "\tWEIRDNESS: " + std::to_string(climate.weirdness) + '\n' + 
            "CURSOR POSITION: " + std::to_string(getMouseWorldPosition(camera, game->getWindow()).x) + " / " + std::to_string(getMouseWorldPosition(camera, game->getWindow()).y) + '\n' +
            "CURSOR BLOCK POSITION: " + std::to_string(getMouseBlockPosition(camera, game->getWindow()).x) + " / " + std::to_string(getMouseBlockPosition(camera, game->getWindow()).y) + '\n';
    }

    return debug_string;
}

void ClientGameState::saveScreenshot(sf::RenderWindow& window)
{
    std::filesystem::path directory;

    #ifdef _WIN32
        const char* appdata = std::getenv("APPDATA");
        directory = appdata ? std::filesystem::path(appdata) : std::filesystem::temp_directory_path();
    #else
        const char* home = std::getenv("HOME");
        directory = home ? std::filesystem::path(home) : std::filesystem::temp_directory_path();
    #endif

    directory /= "Blockbit";
    directory /= "screenshots";

    try
    {
        std::filesystem::create_directories(directory);
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[Client] Failed to create screenshot directory: " << exception.what() << '\n';
        return;
    }

    auto now = std::chrono::system_clock::now();
    std::time_t time_now = std::chrono::system_clock::to_time_t(now);
    std::tm time_local{};
    #ifdef _WIN32
        localtime_s(&time_local, &time_now);
    #else
        localtime_r(&time_now, &time_local);
    #endif

    std::ostringstream file_name;
    file_name << "screenshot_" << std::put_time(&time_local, "%Y-%m-%d_%H-%M-%S") << ".png";

    sf::Vector2u window_size = window.getSize();
    if (window_size.x == 0 || window_size.y == 0) return;

    sf::Texture texture(window_size);
    texture.update(window);

    std::filesystem::path file_path = directory / file_name.str();
    if (!texture.copyToImage().saveToFile(file_path.string()))
    {
        std::cerr << "[Client] Failed to save screenshot to " << file_path << '\n';
        return;
    }

    std::cerr << "[Client] Screenshot saved: " << file_path << '\n';
}

bool ClientGameState::alwaysUpdated() const
{
    return true;
}