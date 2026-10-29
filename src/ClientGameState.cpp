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
#include "../include/ChestView.hpp"
#include "../include/InventoryView.hpp"
#include "../include/FurnaceView.hpp"
#include "../include/CraftingView.hpp"
#include "../include/Settings.hpp"

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
    game->getWindow().setTitle(std::string("Blockbit - ") + (network_port == 0 ? "Singleplayer" : "Host"));

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

    // Slash-prefixed input in Console → server, so operator/requires_* checks
    // live only in GameServer::processIncoming. Local commands (max-fps etc.)
    // still run through the normal commandDatabase path in Console::update.
    game->getConsole().assignCommandRelay([this](const std::wstring& command)
    {
        if (!transport || !transport->isConnected()) return;
        transport->send(serializePacket(CommandMessagePacket{command}));
    });
}

ClientGameState::ClientGameState(Game* game, std::string name, unsigned int seed, GenerationProperties generation_properties, uint16_t network_port, std::string nickname) : GameState(game)
{
    game->getWindow().setTitle(std::string("Blockbit - ") + (network_port == 0 ? "Singleplayer" : "Host"));

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

    // Slash-prefixed input in Console → server, so operator/requires_* checks
    // live only in GameServer::processIncoming. Local commands (max-fps etc.)
    // still run through the normal commandDatabase path in Console::update.
    game->getConsole().assignCommandRelay([this](const std::wstring& command)
    {
        if (!transport || !transport->isConnected()) return;
        transport->send(serializePacket(CommandMessagePacket{command}));
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
    game->getConsole().assignCommandRelay({});
}

bool ClientGameState::hasPlayerEntity() const
{
    if (!local_player_entity_id.has_value()) return false;

    return local_world.doesEntityExist(local_player_entity_id.value());
}

void ClientGameState::tryInitializePlayerUI()
{
    if (player_ui_initialized) return;
    if (!hasPlayerEntity()) return;

    auto& player_entity = local_world.getEntity(local_player_entity_id.value());

    if (!player_entity.hasComponent<TransformComponent>()) return;

    //health-bar

    player_ui_initialized = true;
}

void ClientGameState::applySnapshot(const SnapshotPacket& snapshot)
{
    latest_tick = snapshot.tick;
    time_since_last_snapshot = 0.0f;

    local_world.dayTime = snapshot.dayTime;
    local_world.days = snapshot.days;

    for (const auto& [id, tag] : snapshot.new_entities)
    {
        if (local_world.doesEntityExist(id)) local_world.removeEntity(id);

        Entity entity(id);
        Tag payload = tag;
        entity.deserialize(payload);

        local_world.addEntity(std::move(entity));

        if (local_player_entity_id == id)
        {
            player_ui_initialized = false;
            content_view.reset();
        }
    }

    for (const auto& [id, tag] : snapshot.updated_entities)
    {
        if (!local_world.doesEntityExist(id)) continue;

        auto& entity = local_world.getEntity(id);
        entity.applyUpdate(tag);

        if (local_player_entity_id != id && entity.hasComponent<TransformComponent>())
        {
            if (!entity.hasComponent<NetworkInterpolationComponent>())
                entity.addComponent<NetworkInterpolationComponent>(NetworkInterpolationComponent{});

            const auto& transform = entity.getComponent<TransformComponent>();
            entity.getComponent<NetworkInterpolationComponent>().pushSample(snapshot.tick, transform.position, transform.rotation);
        }
    }

    for (UUID id : snapshot.removed_entities)
    {
        if (!local_world.doesEntityExist(id)) continue;

        if (local_player_entity_id == id)
        {
            player_ui_initialized = false;
            content_view.reset();
        }
        local_world.removeEntity(id);
    }
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
                    local_world.getChunkMeshes()[initialization.chunk.chunk_position].built = false;
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
                    local_world.setBlock(block_update.position, block_update.block);
                    break;
                }
                case PacketType::Track:
                {
                    auto spawn = deserializeTrack(reader);
                    my_entity_id = spawn.id;
                    local_player_entity_id = my_entity_id;
                    break;
                }
                case PacketType::StopTracking:
                {
                    local_player_entity_id = std::nullopt;
                    my_entity_id = UUID();
                    player_ui_initialized = false;
                    content_view.reset();
                    break;
                }
                case PacketType::ChatMessage:
                {
                    auto message = deserializeChatMessage(reader);
                    chat.pushMessage(message.message);
                    // Mirror to the in-game console so the player sees it even
                    // if the chat panel is hidden — matters most for
                    // kick/ban/disconnect notifications fired just before the
                    // connection closes.
                    game->getConsole().writeLine(message.message);
                    break;
                }
                case PacketType::OpenView:
                {
                    auto open = deserializeOpenView(reader);

                    content_view.reset();

                    switch (open.type)
                    {
                        case ViewType::Chest:
                            content_view = std::make_unique<ClientChestView>();
                            break;
                        case ViewType::Inventory:
                            content_view = std::make_unique<ClientInventoryView>();
                            break;
                        case ViewType::Furnace:
                            content_view = std::make_unique<ClientFurnaceView>();
                            break;
                        case ViewType::Crafting:
                            content_view = std::make_unique<ClientCraftingView>();
                            break;
                    }

                    break;
                }
                case PacketType::CloseView:
                {
                    (void)deserializeCloseView(reader);
                    content_view.reset();
                    break;
                }
                case PacketType::SetContent:
                {
                    auto set_content = deserializeSetContent(reader);
                    if (content_view)
                    {
                        content_view->handleContentUpdate(SetContentData{set_content.target, std::move(set_content.payload)});
                    }
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

void ClientGameState::updateViewTransform()
{
    sf::Vector2<double> base_center{0.0, 0.0};
    bool have_player = hasPlayerEntity();

    if (free_camera)
    {
        base_center = free_camera_position;
    }
    else if (have_player)
    {
        base_center = local_world.getEntity(local_player_entity_id.value())
                          .getComponent<TransformComponent>()
                          .position + sf::Vector2<double>(0.5, -0.5);
    }
    else
    {
        camera = base_center;
        view_scale = 1.0f;
        return;
    }

    double scale = 1.0;
    sf::Vector2<double> shift{0.0, 0.0};

    // Zoom-out so you can see what's being pointed at past your reach: when the
    // cursor leaves the reach ring (dig/attack distance + 1) the view scales up
    // toward the cursor direction while the opposite edge stays anchored. Base
    // view center is used to compute cursor world coords so successive frames
    // don't feed shifted-camera values back into the distance calculation.
    sf::Vector2u win = game->getWindow().getSize();
    if (Settings::render.reach_zoom_enabled && !free_camera && have_player && !content_view && win.x > 0 && win.y > 0)
    {
        auto& player_transform = local_world.getEntity(local_player_entity_id.value()).getComponent<TransformComponent>();
        sf::Vector2<double> player_center = player_transform.center();

        double unit = static_cast<double>(win.y) / static_cast<double>(WORLD_UNIT_SIZE_FACTOR);
        sf::Vector2i mouse = sf::Mouse::getPosition(game->getWindow());

        double offset_x = (static_cast<double>(mouse.x) - static_cast<double>(win.x) * 0.5) / unit;
        double offset_y = (static_cast<double>(win.y) * 0.5 - static_cast<double>(mouse.y)) / unit;
        sf::Vector2<double> cursor_world{base_center.x + offset_x, base_center.y + offset_y};

        double dx = cursor_world.x - player_center.x;
        double dy = cursor_world.y - player_center.y;
        double distance = std::sqrt(dx * dx + dy * dy);

        constexpr double REACH_RADIUS = 4.0 + 1.0;
        double strength = static_cast<double>(Settings::render.reach_zoom_scale);

        if (distance > REACH_RADIUS && strength > 0.0)
        {
            double overshoot_ratio = (distance - REACH_RADIUS) / REACH_RADIUS;
            scale = 1.0 + overshoot_ratio * strength;

            double half_w_world = static_cast<double>(win.x) * 0.5 / unit;
            double half_h_world = static_cast<double>(win.y) * 0.5 / unit;
            double inv_dist = 1.0 / distance;

            shift.x = (dx * inv_dist) * (scale - 1.0) * half_w_world;
            shift.y = (dy * inv_dist) * (scale - 1.0) * half_h_world;
        }
    }

    camera = base_center + shift;
    view_scale = static_cast<float>(scale);
}

void ClientGameState::sendTickInputs()
{
    if (!transport || !transport->isConnected()) return;
    if (my_entity_id == UUID()) return;

    if (!free_camera)
    {
        auto polled = getInputs(local_world, camera, game->getWindow());
        inputs.insert(inputs.end(), std::make_move_iterator(polled.begin()), std::make_move_iterator(polled.end()));
    }

    if (inputs.empty()) return;

    // Client-side prediction: apply the player's movement inputs locally
    // before sending them off. PhysicsSystem on the client will then step
    // the player each frame, so motion is responsive instead of waiting
    // one round-trip for a snapshot.
    if (local_player_entity_id.has_value())
    {
        applyLocalPlayerInputPrediction(local_world, inputs, local_player_entity_id.value());
    }

    InputPacket packet;
    packet.id = my_entity_id;
    packet.inputs = std::move(inputs);
    inputs.clear();

    transport->send(serializePacket(packet));
}

static void drainViewEventsAndSend(ClientView* view, ClientTransport* transport)
{
    if (!view || !transport || !transport->isConnected()) return;

    auto events = view->drainPendingEvents();
    for (auto& event_data : events)
    {
        ViewEventPacket packet;
        packet.sender  = event_data.sender;
        packet.action  = event_data.action;
        packet.payload = std::move(event_data.payload);

        transport->send(serializePacket(packet));
    }
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

    drainViewEventsAndSend(content_view.get(), transport.get());

    if (local_server)
    {
        local_server->update(tick_step);

        if (local_server->isUnrecoverable())
        {
            server_unrecoverable = true;
            return;
        }
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

    if (content_view)
    {
        if (auto key = event.getIf<sf::Event::KeyPressed>())
        {
            if (key->code == sf::Keyboard::Key::Escape || key->code == sf::Keyboard::Key::E)
            {
                if (transport && transport->isConnected())
                {
                    transport->send(serializePacket(CloseViewPacket{}));
                }
                content_view.reset();
                view_close_cooldown = 0.3f;
                return;
            }
        }

        content_view->handleEvent(event);
        return;
    }

    if (auto key = event.getIf<sf::Event::KeyPressed>())
    {
        if (!chat_was_active && key->code == sf::Keyboard::Key::T)
        {
            chat_ui.open();
            return;
        }

        // Escape (and Android's hardware back, which SFML maps to Escape)
        // opens the pause menu. Also covered by the polling path below for
        // desktop key-hold ergonomics, but the event path is required on
        // Android where Back fires once per press.
        if (key->code == sf::Keyboard::Key::Escape &&
            view_close_cooldown <= 0.0f &&
            !chat_ui.isActive())
        {
            game->pushState(this, std::make_unique<PauseScreenState>(game, this));
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
        local_world.getChunkMeshes().clear();
    }


    sf::View game_view({0.0f, 0.0f}, {static_cast<float>(game->getWindow().getSize().x), static_cast<float>(game->getWindow().getSize().y)});
    game_view.setSize({game_view.getSize().x, -game_view.getSize().y});
    game->getWindow().setView(game_view);

    auto new_inputs = ::getInputsFromEvent(event, camera, game->getWindow());
    inputs.insert(inputs.end(), std::make_move_iterator(new_inputs.begin()), std::make_move_iterator(new_inputs.end()));

    if(local_server.has_value()) rcon.handleEvent(event);
}

void ClientGameState::update(float dt)
{
    if (server_unrecoverable)
    {
        std::string title = "Server crashed";
        std::string reason = "Reason: too many consecutive tick exceptions";
        std::string hint = "Press ESC to go back";

        Game* temp_game = game;
        local_server.reset();
        game->popState(this);

        game->pushState(this, std::make_unique<AnnouncementState>(temp_game, title + '\n' + reason + '\n' + hint));

        return;
    }

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
    if (view_close_cooldown > 0.0f) view_close_cooldown -= dt;

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

    float tick_step = 1.0f / static_cast<float>(tick_rate);

    if (local_server && !onTop())
    {
        since_last_tick = 0.0f;
        return;
    }

    since_last_tick += dt;

    uint32_t max_ticks_this_frame = local_server ? local_server->getMaxTicksPerFrame() : 10u;
    uint32_t ticks_executed = 0;

    while(since_last_tick >= tick_step)
    {
        if (ticks_executed >= max_ticks_this_frame)
        {
            since_last_tick = 0.0f;
            break;
        }
        ++ticks_executed;

        updateViewTransform();

        if (player_ui_initialized)
        {
            float unit_size = game->getWindow().getView().getSize().y / static_cast<float>(WORLD_UNIT_SIZE_FACTOR);

            sf::View view{
                sf::Vector2f{
                    static_cast<float>(camera.x) * unit_size,
                    static_cast<float>(camera.y) * unit_size
                },
                sf::Vector2f{
                    static_cast<float>(game->getWindow().getSize().x) * view_scale,
                    static_cast<float>(game->getWindow().getSize().y) * view_scale
                }
            };

            view.setSize({view.getSize().x, -view.getSize().y});

            game->getWindow().setView(view);
        }

        onTick(tick_step);

        sf::View game_view(
            {0.0f, 0.0f},
            {
                static_cast<float>(game->getWindow().getSize().x) * view_scale,
                static_cast<float>(game->getWindow().getSize().y) * view_scale
            }
        );
        game_view.setSize({game_view.getSize().x, -game_view.getSize().y});
        game->getWindow().setView(game_view);

        ClientSnapshotPacket packet = {.cursor_x = getMouseWorldPosition(camera, game->getWindow()).x, .cursor_y = getMouseWorldPosition(camera, game->getWindow()).y};

        transport->send(serializePacket(packet));

        since_last_tick -= tick_step;
    }

    if (since_last_tick > tick_step) since_last_tick -= dt;

    PhysicsSystem(local_world, dt);

    for (auto& [id, entity] : local_world.getEntities())
    {
        if (!entity.hasComponent<HealthComponent>()) continue;
        auto& health = entity.getComponent<HealthComponent>();
        if (health.since_last_damaged < HealthComponent::DAMAGE_FLASH_DURATION)
        {
            health.since_last_damaged = std::min(health.since_last_damaged + dt, HealthComponent::DAMAGE_FLASH_DURATION);
        }
    }

    AnimationSystem(local_world, dt);

    furnace_animation_timer += dt;
    if (furnace_animation_timer >= 1.0f)
    {
        furnace_animation_timer -= 1.0f;

        for (auto& [chunk_position, chunk] : local_world.getChunks())
        {
            for (int y = 0; y < CHUNK_HEIGHT; ++y)
            {
                for (int x = 0; x < CHUNK_WIDTH; ++x)
                {
                    if (chunk.blocks[y][x].id == BlockID::Furnace &&
                        !FurnaceState::unpack(chunk.blocks[y][x].metadata).lit)
                    {
                        chunk.meshDirty = true;
                        goto next_chunk;
                    }
                }
            }
            next_chunk:;
        }
    }

    // Advance the fractional render tick with real time so interpolation
    // keeps producing fresh positions between server snapshots.
    time_since_last_snapshot += dt;
    const double tick_step_d = static_cast<double>(getTickStep(tick_rate));
    const double render_tick = static_cast<double>(latest_tick)
                             + static_cast<double>(time_since_last_snapshot) / tick_step_d
                             - INTERPOLATION_DELAY_TICKS;

    NetworkInterpolationSystem(local_world, render_tick);

    game->getConsole().assignWorld(local_server.has_value() ? &local_server->getWorld() : &local_world);

    if(acceptsPlayerInput() && !content_view && view_close_cooldown <= 0.0f && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape))
    {
        game->pushState(this, std::make_unique<PauseScreenState>(game, this));
    }

    if (content_view) content_view->update(dt);

    if(acceptsPlayerInput() && InputManager::isLazyKeyPressed(sf::Keyboard::Key::F3))
    {
        debug = !debug;
    }

    if(acceptsPlayerInput() && InputManager::isLazyKeyPressed(sf::Keyboard::Key::F4))
    {
        free_camera = !free_camera;
        if(free_camera) free_camera_position = camera;
    }

    if(acceptsPlayerInput() && free_camera)
    {
        sf::Vector2<double> direction{0.0, 0.0};
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) direction.x -= 1.0;
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) direction.x += 1.0;
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) direction.y += 1.0;
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) direction.y -= 1.0;
        double speed = FREE_CAMERA_SPEED * (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ? 3.0 : 1.0);
        free_camera_position.x += direction.x * speed * dt;
        free_camera_position.y += direction.y * speed * dt;
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
        auto [sky_top, sky_bottom] = getSkyGradient(solarPhase(local_world.getDayTime(), local_world.days));
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


    // UPDATING CAMERA
    updateViewTransform();

    // GAME RENDER
    sf::View game_view(
        {0.0f, 0.0f},
        {
            static_cast<float>(window.getSize().x) * view_scale,
            static_cast<float>(window.getSize().y) * view_scale
        }
    );
    game_view.setSize({game_view.getSize().x, -game_view.getSize().y});
    window.setView(game_view);

    // SKY
    const float sun_phase = solarPhase(local_world.getDayTime(), local_world.days);
    auto [sky_top, sky_bottom] = getSkyGradient(sun_phase);
    renderSky(window, sky_top, sky_bottom);

    renderStars(sun_phase, window);

    // SUN AND MOON
    renderSunAndMoon(sun_phase, window);

    // ENTITIES
    RenderEntities(local_world, camera, window);

    // WORLD
    RenderWorld(local_world, camera, window);

    // WORLD BORDER (impassable wall at World::BORDER_X)
    RenderBorder(camera, view_scale, window);

    // DESTRUCTION OVERLAY (Perlin-noise cracks over blocks being mined)
    RenderBlockDestruction(local_world, camera, window);

    // BLOCK OUTLINE
    if (hasPlayerEntity())
    {
        RenderBlockOutline(local_world, camera, getMouseBlockPosition(camera, window), local_player_entity_id.value(), window);
    }

    // DEBUG OVERLAYS
    if (Settings::debug.render_hitboxes)      RenderEntityHitboxes(local_world, camera, window);
    if (Settings::debug.render_chunk_borders) RenderChunkBorders(camera, window);


    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));

    if (!hide_ui)
    {
        if(hasPlayerEntity())
        {
            auto& health = local_world.getEntity(local_player_entity_id.value()).getComponent<HealthComponent>();
            auto& inventory = local_world.getEntity(local_player_entity_id.value()).getComponent<InventoryComponent>();

            sf::FloatRect bounds
            {
                {50.0f, static_cast<float>(window.getSize().y) - static_cast<float>(window.getSize().x) * 0.025f - 50.0f},
                {static_cast<float>(window.getSize().x) * 0.1f, static_cast<float>(window.getSize().x) * 0.025f}
            };

            renderBar(static_cast<int>(health.maxHealth), static_cast<int>(health.health), sf::Color::Red, sf::Color(166, 28, 46), bounds, window);
            std::array<ItemStack, 9> array;
            std::copy_n(inventory.inventory.slots.begin(), array.size(), array.begin());
            renderHotbar(array, inventory.selectedSlot, window);

            if (content_view) content_view->render(window);

            RenderMinimap(local_world, camera, window);
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

    if(local_server.has_value()) rcon.performImGui(local_server.value());
}

std::string ClientGameState::debugString()
{
    std::string debug_string = "FPS: " + std::to_string(fps) + '\n';

    if (player_ui_initialized)
    {
        auto simulation_range = local_world.getSimulationRangeForEntity(local_player_entity_id.value());
        Climate climate = local_world.climateAt(static_cast<int>(local_world.getEntity(local_player_entity_id.value()).getComponent<TransformComponent>().position.x));

        const Date today = daysToDate(local_world.days);
        const uint16_t daylight_minutes = static_cast<uint16_t>(dayLightFraction(today) * 1440.0f);
        std::ostringstream daylight_stream;
        daylight_stream << std::setw(2) << std::setfill('0') << (daylight_minutes / 60) << ':'
                        << std::setw(2) << std::setfill('0') << (daylight_minutes % 60);

        debug_string +=
            "X: " + std::to_string(local_world.getEntity(local_player_entity_id.value()).getComponent<TransformComponent>().position.x) +
            " Y: " + std::to_string(local_world.getEntity(local_player_entity_id.value()).getComponent<TransformComponent>().position.y) + '\n' +
            "CHUNKS LOADED: " + std::to_string(local_world.getChunks().size()) + '\n' +
            "SIMULATION RANGE: " + std::to_string(simulation_range.first) + " - " + std::to_string(simulation_range.second) + '\n' +
            "INPUTS: " + std::to_string(inputs.size()) + '\n' +
            "TIME: " + dateAndHourString(today, daytimeToHour(local_world.dayTime)) + '\n' +
            "DAY LENGTH: " + daylight_stream.str() + '\n' +
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
    std::filesystem::path directory = getHomePath() / "Blockbit" / "screenshots";

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