#include "../include/GameServer.hpp"

#include "../include/Entity.hpp"
#include "../include/GameCommon.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/PlayerControlledComponent.hpp"
#include "../include/Chunk.hpp"
#include "../include/Packet.hpp"
#include "../include/PlayerControlledSystem.hpp"
#include "../include/AISystem.hpp"
#include "../include/TransformSystem.hpp"
#include "../include/ExplosiveSystem.hpp"
#include "../include/HealthSystem.hpp"
#include "../include/PhysicsSystem.hpp"
#include "../include/InventorySystem.hpp"
#include "../include/ChunkLoadSystem.hpp"
#include "../include/ChunkUnloadSystem.hpp"
#include "../include/DaycycleSystem.hpp"
#include "../include/FallingBlockSystem.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/AnimationSystem.hpp"
#include "../include/MobSpawnSystem.hpp"
#include "../include/Prefab.hpp"
#include "../include/ChestView.hpp"
#include "../include/InventoryView.hpp"
#include "../include/FurnaceBlockEntity.hpp"
#include "../include/Command.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace
{
    // CommandContext that collects reply lines on the server and flushes them
    // back to the issuing client as ChatMessagePackets. Headless stdin uses a
    // different sink (see HeadlessMain.cpp).
    class NetworkCommandSink : public CommandContext
    {
    public:
        NetworkCommandSink(GameServer& server, uint32_t clientId, std::string nickname, World* world)
            : server_ref(server), clientId(clientId), nick(std::move(nickname)), world_ptr(world) {}

        void reply(const std::wstring& line) override
        {
            server_ref.sendSystemMessageTo(clientId, line);
        }

        void reply(const std::wstring& line, sf::Color /*color*/) override
        {
            server_ref.sendSystemMessageTo(clientId, line);
        }

        World* world() override           { return world_ptr; }
        GameServer* server() override     { return &server_ref; }
        std::string senderNickname() const override { return nick; }
        bool senderIsOperator() const override      { return server_ref.isOperator(nick); }

    private:
        GameServer& server_ref;
        uint32_t clientId;
        std::string nick;
        World* world_ptr;
    };

    std::string currentTimestampIso8601()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_utc{};
#if defined(_WIN32)
        gmtime_s(&tm_utc, &t);
#else
        gmtime_r(&t, &tm_utc);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }
}

GameServer::GameServer(std::filesystem::path world_path, std::unique_ptr<ServerTransport> transport) : mode(ServerMode::Listen), server_path(world_path), transport(std::move(transport)), world(world_path)
{
    this->transport->start();
    loadOperators();
    loadBans();
}

GameServer::GameServer(std::string name, unsigned int seed, GenerationProperties generation_properties, std::unique_ptr<ServerTransport> transport) : mode(ServerMode::Listen), server_path(getWorldsPath() / name), transport(std::move(transport)), world(name, getWorldsPath() / name, seed, generation_properties)
{
    this->transport->start();
    loadOperators();
    loadBans();
}

GameServer::GameServer(std::filesystem::path world_path, std::unique_ptr<ServerTransport> transport, ServerMode mode)
    : mode(mode), server_path(world_path), transport(std::move(transport)), world(world_path)
{
    this->transport->start();
    loadOperators();
    loadBans();

    std::cout << "[Server] Dedicated world loaded from " << world_path << '\n';
}

GameServer::GameServer(std::filesystem::path server_root, std::filesystem::path world_path, std::unique_ptr<ServerTransport> transport)
    : mode(ServerMode::Dedicated), server_path(std::move(server_root)), transport(std::move(transport)), world(world_path)
{
    this->transport->start();
    loadOperators();
    loadBans();

    std::cout << "[Server] Dedicated world loaded from " << world_path
              << " (server dir " << server_path << ")\n";
}

GameServer::GameServer(std::filesystem::path server_root, std::string name, unsigned int seed, GenerationProperties generation_properties, std::unique_ptr<ServerTransport> transport)
    : mode(ServerMode::Dedicated), server_path(std::move(server_root)), transport(std::move(transport)),
      world(name, server_path / "worlds" / name, seed, generation_properties)
{
    this->transport->start();
    loadOperators();
    loadBans();

    std::cout << "[Server] Created new dedicated world '" << name
              << "' in " << (server_path / "worlds" / name) << '\n';
}

void GameServer::sendChunkTo(uint32_t client_id, int around_chunk_position)
{
    auto& known_chunks = clients[client_id].known_chunks;

    for (int offset = -World::LAZY_DISTANCE; offset <= World::LAZY_DISTANCE; ++offset)
    {
        int chunk_position = around_chunk_position + offset;
        world.loadOrCreateChunk(chunk_position);

        ChunkPacket initialization;
        initialization.chunk = world.getChunk(chunk_position);
        transport->send(client_id, serializePacket(initialization));
        known_chunks.insert(chunk_position);
    }
}

void GameServer::streamChunksToClients()
{
    for(auto& [client_id, data] : clients)
    {
        if(nickname_to_entity.contains(data.nickname))
        {
            Entity& entity = world.getEntity(nickname_to_entity[data.nickname]);

            if(!entity.hasComponent<TransformComponent>()) continue;

            int player_chunk = entity.getComponent<TransformComponent>().chunkPosition();

            auto& known_chunks = data.known_chunks;
            auto& chunks = world.getChunks();

            for (int chunk_position = player_chunk - World::LAZY_DISTANCE; chunk_position <= player_chunk + World::LAZY_DISTANCE; ++chunk_position)
            {
                if (known_chunks.contains(chunk_position)) continue;
                auto iterator = chunks.find(chunk_position);
                if (iterator == chunks.end() || !iterator->second.generated) continue;

                ChunkPacket initialization;
                initialization.chunk = iterator->second;
                transport->send(client_id, serializePacket(initialization));
                known_chunks.insert(chunk_position);
            }
        }
    }
}

void GameServer::syncConnections()
{
    auto current = transport->clientIds();
    std::unordered_set<uint32_t> current_set(current.begin(), current.end());

    for (uint32_t client_id : current)
    {
        if (!known_clients.contains(client_id))
        {
            known_clients.insert(client_id);
            transport->send(client_id, serializePacket(InitializationPacket{tick_rate}));
            std::cout << "[Server] Client " << client_id << " connected, waiting for Login\n";
        }
    }

    std::vector<uint32_t> gone;
    for (uint32_t client_id : known_clients)
    {
        if (!current_set.contains(client_id)) gone.push_back(client_id);
    }
    for (uint32_t client_id : gone)
    {
        auto it = clients.find(client_id);
        if (it != clients.end())
        {
            if (it->second.content_view)
            {
                it->second.content_view->onClose(world);
                it->second.content_view.reset();
            }
            deactivatePlayerFor(it->second.nickname);
            clients.erase(it);
        }
        known_clients.erase(client_id);
        remote_input_queues.erase(client_id);
    }
}

void GameServer::processIncoming()
{
    auto packets = transport->poll();
    for (auto& packet : packets)
    {
        try
        {
            PacketReader reader(packet.payload.data(), packet.payload.size());

            switch (packet.type)
            {
                case PacketType::Login:
                {
                    LoginPacket login = deserializeLogin(reader);

                    if(!isNicknameAllowed(login.nickname))
                    {
                        std::cerr << "[Server] Login with not allowed nickname: " << login.nickname << " from: " << packet.clientId << '\n';
                        break;
                    }

                    if (!known_clients.contains(packet.clientId))
                    {
                        std::cerr << "[Server] Login from unknown client " << packet.clientId << ", ignoring\n";
                        break;
                    }

                    if (const BanEntry* ban = findBan(login.nickname))
                    {
                        std::wstring wreason(ban->reason.begin(), ban->reason.end());
                        std::wstring message = L"[Server] You are banned";
                        if (!ban->reason.empty()) message += L": " + wreason;

                        sendSystemMessageTo(packet.clientId, message);

                        std::cout << "[Server] Rejected banned nickname "
                                  << login.nickname << " from client " << packet.clientId
                                  << " (reason: " << (ban->reason.empty() ? "<none>" : ban->reason) << ")\n";

                        transport->disconnect(packet.clientId, message);
                        known_clients.erase(packet.clientId);
                        break;
                    }

                    if(!clients.contains(packet.clientId))
                    {
                        clients[packet.clientId].nickname = login.nickname;
                        remote_input_queues[packet.clientId] = {};
                        spawnPlayerFor(login.nickname);

                        UUID entity_id = nickname_to_entity[login.nickname];
                        transport->send(packet.clientId, serializePacket(TrackPacket{entity_id}));

                        std::cout << "[Server] Logged " << packet.clientId << " as " << login.nickname << '\n';
                    }
                    else
                    {
                        std::cerr << "[Server] Another login from " << packet.clientId << ", ignoring\n";
                    }

                    break;
                }
                case PacketType::Input:
                {
                    InputPacket input_packet = deserializeInput(reader);
                    auto iterator = remote_input_queues.find(packet.clientId);
                    if (iterator == remote_input_queues.end()) break;
                    iterator->second.push_back(std::move(input_packet.inputs));
                    break;
                }
                case PacketType::BlockUpdate:
                {
                    BlockUpdatePacket block_update = deserializeBlockUpdate(reader);
                    world.setBlock(block_update.position, block_update.block);
                    break;
                }
                case PacketType::StatusRequest:
                {
                    StatusResponsePacket response;

                    response.name = server_name;
                    response.description = server_description;

                    response.players = static_cast<uint32_t>(clients.size());
                    response.max_players = max_players;

                    response.date = daysToDate(world.days);
                    response.hour = daytimeToHour(world.dayTime);

                    transport->send(packet.clientId, serializePacket(response));

                    break;
                }
                case PacketType::ChatMessage:
                {
                    ChatMessagePacket chat_message = deserializeChatMessage(reader);

                    auto it = clients.find(packet.clientId);
                    if (it == clients.end()) break;

                    const std::string& nickname = it->second.nickname;
                    std::wstring wide_nickname(nickname.begin(), nickname.end());

                    sendChat(wide_nickname, chat_message.message);

                    break;
                }
                case PacketType::CommandMessage:
                {
                    CommandMessagePacket command_message = deserializeCommandMessage(reader);

                    auto it = clients.find(packet.clientId);
                    if (it == clients.end()) break;

                    const std::string& nickname = it->second.nickname;

                    std::wistringstream stream(command_message.command);
                    std::wstring command_name;
                    stream >> command_name;

                    auto cmd_it = commandDatabase.find(command_name);
                    if (cmd_it == commandDatabase.end())
                    {
                        sendSystemMessageTo(packet.clientId, L"No command found: " + command_name);
                        break;
                    }

                    const Command& command_data = cmd_it->second;

                    // Server has no Game*: commands marked requires_game can only
                    // run on the client that owns the window.
                    if (command_data.requires_game)
                    {
                        sendSystemMessageTo(packet.clientId, L"That command is client-only.");
                        break;
                    }

                    if (command_data.requires_operator && !isOperator(nickname))
                    {
                        std::cerr << "[Server] " << nickname << " attempted operator command: "
                                  << std::string(command_message.command.begin(), command_message.command.end()) << '\n';
                        sendSystemMessageTo(packet.clientId, L"You are not an operator.");
                        break;
                    }

                    NetworkCommandSink sink(*this, packet.clientId, nickname, &world);

                    try
                    {
                        command_data.on_call(command_message.command, sink);
                        std::cout << "[Server] " << nickname << " ran command: "
                                  << std::string(command_message.command.begin(), command_message.command.end()) << '\n';
                    }
                    catch (const CommandException& exception)
                    {
                        std::wstring msg(exception.message.begin(), exception.message.end());
                        sendSystemMessageTo(packet.clientId, L"Error: " + msg);
                    }
                    catch (const std::exception& exception)
                    {
                        std::string what = exception.what();
                        std::wstring wwhat(what.begin(), what.end());
                        sendSystemMessageTo(packet.clientId, L"Error: " + wwhat);
                    }

                    break;
                }
                case PacketType::ClientSnapshot:
                {
                    ClientSnapshotPacket client_snapshot = deserializeClientSnapshot(reader);

                    auto client_it = clients.find(packet.clientId);
                    if (client_it == clients.end()) break;

                    auto entity_it = nickname_to_entity.find(client_it->second.nickname);
                    if (entity_it == nickname_to_entity.end()) break;

                    if (!world.doesEntityExist(entity_it->second)) break;

                    auto& player_component = world.getEntity(entity_it->second).getComponent<PlayerControlledComponent>();
                    player_component.cursor_position = {client_snapshot.cursor_x, client_snapshot.cursor_y};
                    player_component.dirty = true;

                    break;
                }
                case PacketType::ViewEvent:
                {
                    ViewEventPacket view_event = deserializeViewEvent(reader);

                    auto client_it = clients.find(packet.clientId);
                    if (client_it == clients.end()) break;

                    auto& view = client_it->second.content_view;
                    if (!view) break;

                    view->handleViewEvent(world, ViewEventData{view_event.sender, view_event.action, std::move(view_event.payload)});

                    break;
                }
                case PacketType::CloseView:
                {
                    (void)deserializeCloseView(reader);

                    auto client_it = clients.find(packet.clientId);
                    if (client_it == clients.end()) break;

                    if (client_it->second.content_view)
                    {
                        client_it->second.content_view->onClose(world);
                        client_it->second.content_view.reset();
                    }

                    break;
                }
                default:
                {
                    std::cerr << "[Server] Unrecognised packet (" << std::to_string(static_cast<uint8_t>(packet.type)) << ") from client " << std::to_string(packet.clientId) << '\n';

                    break;
                }
            }
        }
        catch (const std::exception& exception)
        {
            std::cerr << "[Server] Bad packet (" << std::to_string(static_cast<uint8_t>(packet.type)) << ") from client " << packet.clientId << ": " << exception.what() << '\n';
        }
    }
}

void GameServer::broadcastBlockUpdates()
{
    if (world.pending_block_updates.empty()) return;

    for (const sf::Vector2i& position : world.pending_block_updates)
    {
        BlockUpdatePacket block_update{position, world.getBlock(position.x, position.y)};
        transport->broadcast(serializePacket(block_update));
    }
    world.pending_block_updates.clear();
}

void GameServer::broadcastSnapshot()
{
    if (clients.empty())
    {
        world.new_entities.clear();
        world.dead_entities.clear();
        for (auto& [id, entity] : world.getEntities()) entity.clearDirty();
        return;
    }

    const auto& entities = world.getEntities();

    for (auto& [client_id, data] : clients)
    {
        SnapshotPacket snapshot;
        snapshot.tick = tick;
        snapshot.dayTime = world.getDayTime();
        snapshot.days = world.days;

        for (const auto& [id, entity] : entities)
        {
            if (data.known_entities.contains(id))
            {
                Tag update = entity.serializeDirtyForClient();
                if (!update.get<TagCompound>().empty())
                {
                    snapshot.updated_entities.emplace(id, std::move(update));
                }
            }
            else
            {
                snapshot.new_entities.emplace(id, entity.serializeForClient());
                data.known_entities.insert(id);
            }
        }

        for (UUID id : world.dead_entities)
        {
            if (data.known_entities.erase(id) > 0)
            {
                snapshot.removed_entities.insert(id);
            }
        }

        transport->send(client_id, serializePacket(snapshot));
    }

    world.new_entities.clear();
    world.dead_entities.clear();

    for (auto& [id, entity] : world.getEntities()) entity.clearDirty();
}

void GameServer::dispatchPendingViewOpens()
{
    for (auto& [client_id, data] : clients)
    {
        auto entity_it = nickname_to_entity.find(data.nickname);
        if (entity_it == nickname_to_entity.end()) continue;
        if (!world.doesEntityExist(entity_it->second)) continue;

        auto& entity = world.getEntity(entity_it->second);
        if (!entity.hasComponent<PlayerControlledComponent>()) continue;

        auto& queue = entity.getComponent<PlayerControlledComponent>().pending_view_opens;

        while (!queue.empty())
        {
            std::unique_ptr<ServerView> requested = std::move(queue.front());
            queue.pop_front();
            if (!requested) continue;

            ViewType type = requested->type();

            if (data.content_view) data.content_view->onClose(world);
            data.content_view = std::move(requested);

            transport->send(client_id, serializePacket(OpenViewPacket{type}));

            data.content_view->onOpen(world);

            // Any content updates the view queued in its constructor are
            // flushed in updateContentViews() right after this.
        }
    }
}

void GameServer::updateContentViews()
{
    for (auto& [client_id, data] : clients)
    {
        if (!data.content_view) continue;

        if (data.content_view->shouldClose(world))
        {
            data.content_view->onClose(world);
            data.content_view.reset();
            transport->send(client_id, serializePacket(CloseViewPacket{}));
            continue;
        }

        data.content_view->update(world);

        auto updates = data.content_view->drainPendingContentUpdates();

        for (auto& update : updates)
        {
            SetContentPacket packet;
            packet.target  = update.target;
            packet.payload = std::move(update.payload);

            transport->send(client_id, serializePacket(packet));
        }
    }
}

void GameServer::sendChat(const std::wstring& nickname, const std::wstring& message)
{
    std::wstring final_message = nickname + L"> " + message + L'\n';

    ChatMessagePacket packet;
    packet.message = final_message;
    auto buffer = serializePacket(packet);

    for (auto client_id : known_clients)
    {
        transport->send(client_id, buffer);
    }

    if (onChatBroadcast) onChatBroadcast(final_message);
}


void GameServer::update(float dt)
{
    if (unrecoverable) return;

    try
    {
        update_clock.restart();

        syncConnections();
        processIncoming();

        for (auto& [client_id, queue] : remote_input_queues)
        {
            auto client_it = clients.find(client_id);
            if (client_it == clients.end()) continue;

            while(!queue.empty())
            {
                auto iterator = nickname_to_entity.find(client_it->second.nickname);
                if (iterator == nickname_to_entity.end()) continue;
                if (!world.doesEntityExist(iterator->second)) continue;

                processWorldInputs(world, std::move(queue.front()), iterator->second);
                queue.pop_front();
            }
        }

        ChunkLoadSystem(world, dt);
        AISystem(world, dt);
        ExplosiveSystem(world, dt);
        HealthSystem(world, dt);
        PhysicsSystem(world, dt);
        FallingBlockSystem(world);
        PlayerControlledSystem(world, dt);
        InventorySystem(world);
        ChunkUnloadSystem(world);
        DaycycleSystem(world, dt);
        AnimationSystem(world, dt);

        TransformSystem(world);

        for(auto& [client_id, data] : clients)
        {
            auto it = nickname_to_entity.find(data.nickname);
            if (it != nickname_to_entity.end() && world.doesEntityExist(it->second)) continue;

            if (data.content_view)
            {
                data.content_view->onClose(world);
                data.content_view.reset();
                transport->send(client_id, serializePacket(CloseViewPacket{}));
            }

            respawnPlayerFor(data.nickname);

            transport->send(client_id, serializePacket(TrackPacket{nickname_to_entity[data.nickname]}));
        }

        world.tick(dt);

        for (auto& [chunk_position, chunk] : world.getChunks())
        {
            if (!chunk.generated) continue;
            for (auto& [key, block_entity] : chunk.block_entities)
            {
                if (!block_entity) continue;
                if (block_entity->typeId() != BlockEntityID::Furnace) continue;
                block_entity->tick(world, dt);
            }
        }

        MobSpawnSystem(world, dt);

        tick++;

        updatePlayerEntityConnections();

        dispatchPendingViewOpens();
        updateContentViews();

        // Drain block updates after everything that could have modified blocks
        // (systems, world.tick, block entities). Order matters: propagation first
        // because systems may have called setBlock synchronously, then scheduled
        // ticks (which can cascade propagation again via drainPropagation inside
        // the loop), then random ticks. broadcastBlockUpdates right after picks
        // up every resulting block state within this same tick.
        {
            auto& pipeline = world.getBlockUpdatePipeline();
            pipeline.setCurrentTick(tick);
            pipeline.drainPropagation();
            pipeline.processScheduled();
            pipeline.runRandomTicks();
        }

        streamChunksToClients();
        broadcastBlockUpdates();
        broadcastSnapshot();

        update_clock.stop();

        float time = update_clock.getElapsedTime().asSeconds();

        if(std::max(time - 0.1f, getTickStep()) == time - 0.1f)
        {
            std::cerr << "[Server] Server late in tick: " << tick << " by: " << time - getTickStep() << "s (is server overloaded?)" << '\n';
        }
        else if(std::min(time + 0.1f, getTickStep()) == time + 0.1f)
        {
            std::cerr << "[Server] Server ahead in tick: " << tick << " by: " << getTickStep() - time << "s" << '\n';
        }

        consecutive_tick_exceptions = 0;

        return;
    }
    catch(const std::bad_alloc&)
    {
        ++consecutive_tick_exceptions;
        std::cerr << "[Server] [" << consecutive_tick_exceptions << '/' << MAX_CONSECUTIVE_TICK_EXCEPTIONS
                  << "] bad_alloc in tick\n";
    }
    catch (const std::exception& exception)
    {
        ++consecutive_tick_exceptions;
        std::cerr << "[Server] [" << consecutive_tick_exceptions << '/' << MAX_CONSECUTIVE_TICK_EXCEPTIONS
                  << "] exception in tick: " << exception.what() << '\n';
    }

    if (consecutive_tick_exceptions >= MAX_CONSECUTIVE_TICK_EXCEPTIONS)
    {
        unrecoverable = true;
        std::cerr << "[Server] " << consecutive_tick_exceptions
                  << " consecutive tick exceptions; marking server as unrecoverable\n";
    }
}

void GameServer::spawnPlayerFor(std::string nickname)
{
    if(world.playerFileExist(nickname))
    {
        nickname_to_entity[nickname] = world.loadPlayer(nickname);
    }
    else
    {
        Entity entity = PrefabRegistry::get(PrefabID::Player).cloneAs(generateUUID());

        if(entity.hasComponent<TransformComponent>()) entity.getComponent<TransformComponent>().position = world.getSpawnPoint();

        entity.addComponent(PlayerControlledComponent{nickname});

        nickname_to_entity[nickname] = entity.getID();

        world.addEntity(std::move(entity));
    }

    std::cout << "[Server] Spawned player for " << nickname << '\n';
}

void GameServer::respawnPlayerFor(std::string nickname)
{
    Entity entity = PrefabRegistry::get(PrefabID::Player).cloneAs(generateUUID());

    if(entity.hasComponent<TransformComponent>()) entity.getComponent<TransformComponent>().position = world.getSpawnPoint();

    entity.addComponent(PlayerControlledComponent{nickname});

    nickname_to_entity[nickname] = entity.getID();

    world.addEntity(std::move(entity));

    std::cout << "[Server] Respawned player for " << nickname << '\n';
}

void GameServer::deactivatePlayerFor(std::string nickname)
{
    auto it = nickname_to_entity.find(nickname);
    if (it == nickname_to_entity.end()) return;

    UUID entity_id = it->second;

    if (world.doesEntityExist(entity_id))
    {
        world.savePlayer(entity_id);
        world.removeEntity(entity_id);
    }

    nickname_to_entity.erase(it);

    std::cout << "[Server] Despawned player for " << nickname << '\n';
}
void GameServer::updatePlayerEntityConnections()
{
    for(auto it = nickname_to_entity.begin(); it != nickname_to_entity.end();)
    {
        if(!world.doesEntityExist(it->second))
            it = nickname_to_entity.erase(it);
        else
            ++it;
    }
}
GameServer::~GameServer()
{
    transport->stop();

    std::vector<std::string> connected_nicknames;
    connected_nicknames.reserve(nickname_to_entity.size());

    for (auto& [nickname, entity_id] : nickname_to_entity)
        connected_nicknames.push_back(nickname);

    for (auto& nickname : connected_nicknames)
    {
        try
        {
            deactivatePlayerFor(nickname);
        }
        catch (const std::exception& exception)
        {
            std::cerr << "[Server] Failed to save player " << nickname << " on shutdown: " << exception.what() << '\n';
        }
    }

    if (save_on_destruct)
    {
        try { world.save(); }
        catch (const std::exception& exception)
        {
            std::cerr << "[Server] world.save() failed: " << exception.what() << '\n';
        }
    }
}

bool isNicknameAllowed(std::string nickname)
{
    return nickname.length() >= 4 && nickname.length() <= 15 && nickname.find(" ") == std::string::npos;
}

std::filesystem::path GameServer::operatorsFilePath() const
{
    return server_path / "ops.json";
}

std::filesystem::path GameServer::legacyOperatorsFilePath() const
{
    return server_path / "ops.txt";
}

void GameServer::loadOperators()
{
    operators.clear();

    auto json_path   = operatorsFilePath();
    auto legacy_path = legacyOperatorsFilePath();

    if (std::filesystem::exists(json_path))
    {
        try
        {
            std::ifstream in(json_path);
            nlohmann::json data = nlohmann::json::parse(in, nullptr, true, true);

            if (data.is_array())
            {
                for (const auto& entry : data)
                {
                    if (entry.is_string()) operators.insert(entry.get<std::string>());
                }
            }

            std::cout << "[Server] Loaded " << operators.size() << " operator(s) from " << json_path << '\n';
            return;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Server] Could not parse ops.json: " << e.what() << '\n';
        }
    }

    if (std::filesystem::exists(legacy_path))
    {
        std::ifstream in(legacy_path);
        std::string line;
        while (std::getline(in, line))
        {
            auto first = line.find_first_not_of(" \t\r\n");
            auto last  = line.find_last_not_of(" \t\r\n");
            if (first == std::string::npos) continue;
            std::string trimmed = line.substr(first, last - first + 1);
            if (trimmed.empty() || trimmed[0] == '#') continue;
            operators.insert(std::move(trimmed));
        }

        std::cout << "[Server] Migrated " << operators.size()
                  << " operator(s) from legacy ops.txt to ops.json\n";
        saveOperators();

        std::error_code ec;
        std::filesystem::remove(legacy_path, ec);
        return;
    }

    try
    {
        std::filesystem::create_directories(server_path);
        saveOperators();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Server] Could not create ops.json: " << e.what() << '\n';
    }
}

void GameServer::saveOperators() const
{
    try
    {
        std::filesystem::create_directories(server_path);
        nlohmann::json data = nlohmann::json::array();
        for (const auto& nick : operators) data.push_back(nick);
        std::ofstream out(operatorsFilePath(), std::ios::trunc);
        out << data.dump(4);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Server] Could not persist ops.json: " << e.what() << '\n';
    }
}

bool GameServer::isOperator(const std::string& nickname) const
{
    return operators.contains(nickname);
}

void GameServer::addOperator(const std::string& nickname)
{
    if (operators.insert(nickname).second) saveOperators();
}

void GameServer::removeOperator(const std::string& nickname)
{
    if (operators.erase(nickname) > 0) saveOperators();
}

std::filesystem::path GameServer::bansFilePath() const
{
    return server_path / "banned.json";
}

void GameServer::loadBans()
{
    bans.clear();

    auto path = bansFilePath();

    if (!std::filesystem::exists(path))
    {
        try
        {
            std::filesystem::create_directories(server_path);
            saveBans();
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Server] Could not create banned.json: " << e.what() << '\n';
        }
        return;
    }

    try
    {
        std::ifstream in(path);
        nlohmann::json data = nlohmann::json::parse(in, nullptr, true, true);

        if (!data.is_array())
        {
            std::cerr << "[Server] banned.json is not a JSON array; ignoring\n";
            return;
        }

        for (const auto& entry : data)
        {
            if (!entry.is_object()) continue;
            if (!entry.contains("nickname")) continue;

            BanEntry be;
            be.nickname  = entry.value("nickname",  std::string());
            be.reason    = entry.value("reason",    std::string());
            be.banned_at = entry.value("banned_at", std::string());

            if (be.nickname.empty()) continue;
            bans[be.nickname] = std::move(be);
        }

        std::cout << "[Server] Loaded " << bans.size() << " ban(s) from " << path << '\n';
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Server] Could not parse banned.json: " << e.what() << '\n';
    }
}

void GameServer::saveBans() const
{
    try
    {
        std::filesystem::create_directories(server_path);

        nlohmann::json data = nlohmann::json::array();
        for (const auto& [nick, entry] : bans)
        {
            data.push_back({
                {"nickname",  entry.nickname},
                {"reason",    entry.reason},
                {"banned_at", entry.banned_at}
            });
        }

        std::ofstream out(bansFilePath(), std::ios::trunc);
        out << data.dump(4);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Server] Could not persist banned.json: " << e.what() << '\n';
    }
}

bool GameServer::isBanned(const std::string& nickname) const
{
    return bans.contains(nickname);
}

const BanEntry* GameServer::findBan(const std::string& nickname) const
{
    auto it = bans.find(nickname);
    if (it == bans.end()) return nullptr;
    return &it->second;
}

void GameServer::addBan(const std::string& nickname, const std::string& reason)
{
    BanEntry entry;
    entry.nickname  = nickname;
    entry.reason    = reason;
    entry.banned_at = currentTimestampIso8601();
    bans[nickname]  = std::move(entry);
    saveBans();
}

bool GameServer::removeBan(const std::string& nickname)
{
    if (bans.erase(nickname) > 0)
    {
        saveBans();
        return true;
    }
    return false;
}

std::optional<uint32_t> GameServer::findClientIdByNickname(const std::string& nickname) const
{
    for (const auto& [client_id, data] : clients)
    {
        if (data.nickname == nickname) return client_id;
    }
    return std::nullopt;
}

std::optional<UUID> GameServer::findEntityByNickname(const std::string& nickname) const
{
    auto it = nickname_to_entity.find(nickname);
    if (it == nickname_to_entity.end()) return std::nullopt;
    if (!world.doesEntityExist(it->second)) return std::nullopt;
    return it->second;
}

bool GameServer::kickClient(const std::string& nickname, const std::wstring& reason)
{
    auto client_id = findClientIdByNickname(nickname);
    if (!client_id.has_value()) return false;

    std::wstring message = L"[Server] You have been disconnected";
    if (!reason.empty()) message += L": " + reason;

    sendSystemMessageTo(*client_id, message);
    transport->disconnect(*client_id, message);
    return true;
}

void GameServer::sendSystemMessageTo(uint32_t clientId, const std::wstring& message)
{
    ChatMessagePacket packet;
    packet.message = message + L'\n';
    transport->send(clientId, serializePacket(packet));
}


uint16_t GameServer::getTickRate() const
{
    return tick_rate;
}

float GameServer::getTickStep() const
{
    return 1.0f / static_cast<float>(tick_rate);
}