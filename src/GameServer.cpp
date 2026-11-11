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
#include "../include/GameRegistries.hpp"
#include "../include/PlayerControlledSystem.hpp"
#include "../include/AISystem.hpp"
#include "../include/TransformSystem.hpp"
#include "../include/ExplosiveSystem.hpp"
#include "../include/HealthSystem.hpp"
#include "../include/StaminaSystem.hpp"
#include "../include/EffectsSystem.hpp"
#include "../include/PhysicsSystem.hpp"
#include "../include/InventorySystem.hpp"
#include "../include/ChunkLoadSystem.hpp"
#include "../include/ChunkUnloadSystem.hpp"
#include "../include/DaycycleSystem.hpp"
#include "../include/WeatherSystem.hpp"
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

GameServer::GameServer(std::filesystem::path world_path, std::unique_ptr<ServerTransport> transport) : mode(ServerMode::Listen), server_path(world_path), world(world_path), transport(std::move(transport))
{
    this->transport->start();
    loadOperators();
    loadBans();
}

GameServer::GameServer(std::string name, unsigned int seed, GenerationProperties generation_properties, std::unique_ptr<ServerTransport> transport) : mode(ServerMode::Listen), server_path(getWorldsPath() / name), world(name, getWorldsPath() / name, seed, generation_properties), transport(std::move(transport))
{
    this->transport->start();
    loadOperators();
    loadBans();
}

GameServer::GameServer(std::filesystem::path world_path, std::unique_ptr<ServerTransport> transport, ServerMode mode)
    : mode(mode), server_path(world_path), world(world_path), transport(std::move(transport))
{
    this->transport->start();
    loadOperators();
    loadBans();

    std::cout << "[Server] Dedicated world loaded from " << world_path << '\n';
}

GameServer::GameServer(std::filesystem::path server_root, std::filesystem::path world_path, std::unique_ptr<ServerTransport> transport)
    : mode(ServerMode::Dedicated), server_path(std::move(server_root)), world(world_path), transport(std::move(transport))
{
    this->transport->start();
    loadOperators();
    loadBans();

    std::cout << "[Server] Dedicated world loaded from " << world_path
              << " (server dir " << server_path << ")\n";
}

GameServer::GameServer(std::filesystem::path server_root, std::string name, unsigned int seed, GenerationProperties generation_properties, std::unique_ptr<ServerTransport> transport)
    : mode(ServerMode::Dedicated), server_path(std::move(server_root)),
      world(name, server_path / "worlds" / name, seed, generation_properties),
      transport(std::move(transport))
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

    for (int offset = -ServerWorld::LAZY_DISTANCE; offset <= ServerWorld::LAZY_DISTANCE; ++offset)
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
        if(identity_to_entity.contains(data.identity.id))
        {
            Entity& entity = world.getEntity(identity_to_entity[data.identity.id]);

            if(!entity.hasComponent<TransformComponent>()) continue;

            int player_chunk = entity.getComponent<TransformComponent>().chunkPosition();

            auto& known_chunks = data.known_chunks;
            auto& chunks = world.getChunks();

            for (int chunk_position = player_chunk - ServerWorld::LAZY_DISTANCE; chunk_position <= player_chunk + ServerWorld::LAZY_DISTANCE; ++chunk_position)
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

            // Snapshot of the live block+item registries, sent once per client
            // right after init so every runtime id used in later chunk / snapshot
            // packets is already known to the client.
            RegistrySyncPacket sync;
            {
                const auto& reg = gameRegistries();
                sync.blocks.reserve(reg.blocks.size());
                for (RuntimeID id = 0; id < reg.blocks.size(); ++id)
                {
                    if (!reg.blocks.contains(id)) continue;
                    sync.blocks.push_back({reg.blocks.keyOf(id), id});
                }
                sync.items.reserve(reg.items.size());
                for (RuntimeID id = 0; id < reg.items.size(); ++id)
                {
                    if (!reg.items.contains(id)) continue;
                    sync.items.push_back({reg.items.keyOf(id), id});
                }
            }
            transport->send(client_id, serializePacket(sync));

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
            PlayerIdentity identity = it->second.identity;
            deactivatePlayerFor(identity);
            clients.erase(it);
            known_clients.erase(client_id);
            remote_input_queues.erase(client_id);

            std::wstring wide_nickname(identity.display_name.begin(), identity.display_name.end());
            sendSystemMessageToAll(L"[color=#FFFF00]" + wide_nickname + L" left the game[/color]");
            continue;
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

                    if(!isNicknameAllowed(login.identity.display_name))
                    {
                        std::cerr << "[Server] Login with not allowed nickname: " << login.identity.display_name << " from: " << packet.clientId << '\n';
                        break;
                    }

                    // A zero id should never leave the client; if it does the
                    // handshake is malformed. Defensively reject so the id
                    // field never carries an "unassigned" sentinel server-side.
                    if (login.identity.id == 0)
                    {
                        std::cerr << "[Server] Login with id=0 from client " << packet.clientId << ", rejecting\n";
                        break;
                    }

                    if (!known_clients.contains(packet.clientId))
                    {
                        std::cerr << "[Server] Login from unknown client " << packet.clientId << ", ignoring\n";
                        break;
                    }

                    if (const BanEntry* ban = findBan(login.identity.display_name))
                    {
                        std::wstring wreason(ban->reason.begin(), ban->reason.end());
                        std::wstring message = L"[Server] You are banned";
                        if (!ban->reason.empty()) message += L": " + wreason;

                        sendSystemMessageTo(packet.clientId, message);

                        std::cout << "[Server] Rejected banned nickname "
                                  << login.identity.display_name << " from client " << packet.clientId
                                  << " (reason: " << (ban->reason.empty() ? "<none>" : ban->reason) << ")\n";

                        transport->disconnect(packet.clientId, message);
                        known_clients.erase(packet.clientId);
                        break;
                    }

                    if(!clients.contains(packet.clientId))
                    {
                        clients[packet.clientId].identity = login.identity;
                        remote_input_queues[packet.clientId] = {};
                        spawnPlayerFor(login.identity);

                        UUID entity_id = identity_to_entity[login.identity.id];
                        transport->send(packet.clientId, serializePacket(TrackPacket{entity_id}));

                        std::cout << "[Server] Logged " << packet.clientId << " as " << login.identity.display_name << '\n';

                        std::wstring wide_nickname(login.identity.display_name.begin(), login.identity.display_name.end());
                        sendSystemMessageToAll(L"[color=#FFFF00]" + wide_nickname + L" joined the game[/color]");
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
                    iterator->second.push_back(QueuedInputBatch{input_packet.seq, std::move(input_packet.inputs)});
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

                    const std::string& nickname = it->second.identity.display_name;
                    std::wstring wide_nickname(nickname.begin(), nickname.end());

                    sendChat(wide_nickname, chat_message.message);

                    break;
                }
                case PacketType::CommandMessage:
                {
                    CommandMessagePacket command_message = deserializeCommandMessage(reader);

                    auto it = clients.find(packet.clientId);
                    if (it == clients.end()) break;

                    const std::string& nickname = it->second.identity.display_name;

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

                    auto entity_it = identity_to_entity.find(client_it->second.identity.id);
                    if (entity_it == identity_to_entity.end()) break;

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
                case PacketType::AdminRequest:
                {
                    AdminRequestPacket request = deserializeAdminRequest(reader);

                    auto client_it = clients.find(packet.clientId);
                    if (client_it == clients.end()) break;

                    handleAdminRequest(packet.clientId, client_it->second.identity.display_name, request);
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

void GameServer::broadcastParticleBursts()
{
    if (world.pending_particle_bursts.empty()) return;

    for (const ParticleBurstPacket& burst : world.pending_particle_bursts)
    {
        transport->broadcast(serializePacket(burst));
    }
    world.pending_particle_bursts.clear();
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
        snapshot.last_input_seq = data.last_input_seq_applied;
        snapshot.dayTime = world.getDayTime();
        snapshot.days = world.days;
        snapshot.weather = world.weather;

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
        auto entity_it = identity_to_entity.find(data.identity.id);
        if (entity_it == identity_to_entity.end()) continue;
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

        constexpr size_t INPUT_QUEUE_TARGET   = 1;
        constexpr size_t INPUT_QUEUE_GRACE    = 2;
        constexpr size_t INPUT_QUEUE_HARD_CAP = 10;

        for (auto& [client_id, queue] : remote_input_queues)
        {
            auto client_it = clients.find(client_id);
            if (client_it == clients.end()) continue;

            auto iterator = identity_to_entity.find(client_it->second.identity.id);
            if (iterator == identity_to_entity.end()) continue;
            if (!world.doesEntityExist(iterator->second)) continue;

            if (queue.size() > INPUT_QUEUE_HARD_CAP)
            {
                // Dropped batches are still "ack'd" as far as the client is
                // concerned — without this, reconciliation would replay the
                // dropped inputs forever since the server never confirms them.
                while (queue.size() > INPUT_QUEUE_TARGET)
                {
                    if (queue.front().seq > client_it->second.last_input_seq_applied)
                        client_it->second.last_input_seq_applied = queue.front().seq;
                    queue.pop_front();
                }
            }

            size_t to_consume = 1;
            if (queue.size() > INPUT_QUEUE_TARGET + INPUT_QUEUE_GRACE) to_consume = 2;

            for (size_t i = 0; i < to_consume && !queue.empty(); ++i)
            {
                auto& batch = queue.front();
                if (batch.seq > client_it->second.last_input_seq_applied)
                    client_it->second.last_input_seq_applied = batch.seq;
                processWorldInputs(world, std::move(batch.inputs), iterator->second);
                queue.pop_front();
            }
        }

        ChunkLoadSystem(world, dt);
        AISystem(world, dt);
        ExplosiveSystem(world, dt);
        EffectsSystem(world, dt);
        HealthSystem(world, dt);
        StaminaSystem(world, dt);
        PhysicsSystem(world, dt);
        FallingBlockSystem(world);
        PlayerControlledSystem(world, dt);
        InventorySystem(world);
        ChunkUnloadSystem(world);
        DaycycleSystem(world, dt);
        WeatherSystem(world, dt);
        AnimationSystem(world, dt);

        TransformSystem(world);

        for(auto& [client_id, data] : clients)
        {
            auto it = identity_to_entity.find(data.identity.id);
            if (it != identity_to_entity.end() && world.doesEntityExist(it->second)) continue;

            if (data.content_view)
            {
                data.content_view->onClose(world);
                data.content_view.reset();
                transport->send(client_id, serializePacket(CloseViewPacket{}));
            }

            respawnPlayerFor(data.identity);

            transport->send(client_id, serializePacket(TrackPacket{identity_to_entity[data.identity.id]}));
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
        //
        // Gated at a fixed 20 Hz (BLOCK_UPDATE_STEP) so block updates stay on a
        // constant cadence regardless of server tick_rate. If more than one
        // step has accumulated (server stall / slow frame) we catch up, capped
        // at max_ticks_per_frame iterations to avoid runaway work.
        block_update_accumulator += dt;
        {
            auto& pipeline = world.getBlockUpdatePipeline();
            uint32_t iterations = 0;
            while (block_update_accumulator >= BLOCK_UPDATE_STEP && iterations < max_ticks_per_frame)
            {
                block_update_accumulator -= BLOCK_UPDATE_STEP;
                ++block_update_tick;
                ++iterations;

                pipeline.setCurrentTick(block_update_tick);
                pipeline.drainPropagation();
                pipeline.processScheduled();
                pipeline.runRandomTicks();
            }
            if (block_update_accumulator >= BLOCK_UPDATE_STEP)
            {
                // Hit the catch-up cap; discard the backlog to avoid snowballing.
                block_update_accumulator = 0.0f;
            }
        }

        streamChunksToClients();
        broadcastBlockUpdates();
        broadcastSnapshot();
        broadcastParticleBursts();

        item_instance_gc_accumulator += dt;
        if (item_instance_gc_accumulator >= ITEM_INSTANCE_GC_INTERVAL)
        {
            item_instance_gc_accumulator = 0.0f;
            runItemInstanceGC();
        }

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

void GameServer::spawnPlayerFor(const PlayerIdentity& identity)
{
    if(world.playerFileExist(identity))
    {
        identity_to_entity[identity.id] = world.loadPlayer(identity);
    }
    else
    {
        Entity entity = PrefabRegistry::get(PrefabID::Player).cloneAs(generateUUID());

        if(entity.hasComponent<TransformComponent>()) entity.getComponent<TransformComponent>().position = world.getSpawnPoint();

        entity.addComponent(PlayerControlledComponent{identity});

        identity_to_entity[identity.id] = entity.getID();

        world.addEntity(std::move(entity));
    }

    std::cout << "[Server] Spawned player for " << identity.display_name << '\n';
}

void GameServer::respawnPlayerFor(const PlayerIdentity& identity)
{
    Entity entity = PrefabRegistry::get(PrefabID::Player).cloneAs(generateUUID());

    if(entity.hasComponent<TransformComponent>()) entity.getComponent<TransformComponent>().position = world.getSpawnPoint();

    entity.addComponent(PlayerControlledComponent{identity});

    identity_to_entity[identity.id] = entity.getID();

    world.addEntity(std::move(entity));

    std::cout << "[Server] Respawned player for " << identity.display_name << '\n';
}

void GameServer::deactivatePlayerFor(const PlayerIdentity& identity)
{
    auto it = identity_to_entity.find(identity.id);
    if (it == identity_to_entity.end()) return;

    UUID entity_id = it->second;

    if (world.doesEntityExist(entity_id))
    {
        world.savePlayer(entity_id);
        world.removeEntity(entity_id);
    }

    identity_to_entity.erase(it);

    std::cout << "[Server] Despawned player for " << identity.display_name << '\n';
}
void GameServer::updatePlayerEntityConnections()
{
    for(auto it = identity_to_entity.begin(); it != identity_to_entity.end();)
    {
        if(!world.doesEntityExist(it->second))
            it = identity_to_entity.erase(it);
        else
            ++it;
    }
}
GameServer::~GameServer()
{
    transport->stop();

    // Snapshot connected identities before deactivating — deactivatePlayerFor
    // mutates identity_to_entity, so iterating it directly would invalidate.
    std::vector<PlayerIdentity> connected;
    connected.reserve(identity_to_entity.size());

    for (auto& [id, entity_id] : identity_to_entity)
    {
        PlayerIdentity ident{id, ""};
        if (world.doesEntityExist(entity_id))
        {
            auto& entity = world.getEntity(entity_id);
            if (entity.hasComponent<PlayerControlledComponent>())
                ident.display_name = entity.getComponent<PlayerControlledComponent>().identity.display_name;
        }
        connected.push_back(std::move(ident));
    }

    for (auto& identity : connected)
    {
        try
        {
            deactivatePlayerFor(identity);
        }
        catch (const std::exception& exception)
        {
            std::cerr << "[Server] Failed to save player " << identity.display_name << " on shutdown: " << exception.what() << '\n';
        }
    }

    if (save_on_destruct)
    {
        try
        {
            runItemInstanceGC();
            world.save();
        }
        catch (const std::exception& exception)
        {
            std::cerr << "[Server] world.save() failed: " << exception.what() << '\n';
        }
    }
}

void GameServer::runItemInstanceGC()
{
    std::unordered_set<UUID> live;
    for (const auto& [client_id, data] : clients)
    {
        if (data.content_view) data.content_view->collectLiveInstanceIDs(live);
    }
    world.garbageCollectItemInstances(live);
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

    auto hex_to_id = [](const std::string& s) -> uint64_t
    {
        try { return std::stoull(s, nullptr, 16); }
        catch (...) { return 0; }
    };

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
                    // Legacy entries are bare strings (pre-identity ops.json);
                    // new entries are {"id": "<hex>", "nickname": "<hint>"}.
                    if (entry.is_string())
                    {
                        std::string name = entry.get<std::string>();
                        operators[hashDisplayName(name)] = std::move(name);
                    }
                    else if (entry.is_object())
                    {
                        std::string name = entry.value("nickname", std::string());
                        std::string id_hex = entry.value("id", std::string());
                        uint64_t id = hex_to_id(id_hex);
                        if (id == 0) id = hashDisplayName(name);
                        if (id != 0) operators[id] = std::move(name);
                    }
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
            operators[hashDisplayName(trimmed)] = std::move(trimmed);
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
        for (const auto& [id, hint] : operators)
        {
            data.push_back({
                {"id",       identityIdToHex(id)},
                {"nickname", hint}
            });
        }
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
    return operators.contains(hashDisplayName(nickname));
}

void GameServer::addOperator(const std::string& nickname)
{
    uint64_t id = hashDisplayName(nickname);
    auto [it, inserted] = operators.try_emplace(id, nickname);
    if (inserted)
    {
        saveOperators();
    }
    else if (it->second != nickname)
    {
        // Same id already present with a different display-name hint; update
        // the hint so the admin UI shows the latest casing.
        it->second = nickname;
        saveOperators();
    }
}

void GameServer::removeOperator(const std::string& nickname)
{
    if (operators.erase(hashDisplayName(nickname)) > 0) saveOperators();
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

    auto hex_to_id = [](const std::string& s) -> uint64_t
    {
        try { return std::stoull(s, nullptr, 16); }
        catch (...) { return 0; }
    };

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

            std::string id_hex = entry.value("id", std::string());
            be.identity_id = hex_to_id(id_hex);
            // Pre-identity entries lack "id"; fall back to the deterministic
            // hash of the recorded nickname so the ban still catches them.
            if (be.identity_id == 0) be.identity_id = hashDisplayName(be.nickname);

            if (be.identity_id == 0) continue;
            bans[be.identity_id] = std::move(be);
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
        for (const auto& [id, entry] : bans)
        {
            data.push_back({
                {"id",        identityIdToHex(entry.identity_id)},
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
    return bans.contains(hashDisplayName(nickname));
}

const BanEntry* GameServer::findBan(const std::string& nickname) const
{
    auto it = bans.find(hashDisplayName(nickname));
    if (it == bans.end()) return nullptr;
    return &it->second;
}

void GameServer::addBan(const std::string& nickname, const std::string& reason)
{
    BanEntry entry;
    entry.identity_id = hashDisplayName(nickname);
    entry.nickname    = nickname;
    entry.reason      = reason;
    entry.banned_at   = currentTimestampIso8601();
    bans[entry.identity_id] = std::move(entry);
    saveBans();
}

bool GameServer::removeBan(const std::string& nickname)
{
    if (bans.erase(hashDisplayName(nickname)) > 0)
    {
        saveBans();
        return true;
    }
    return false;
}

std::vector<BanEntry> GameServer::listBans() const
{
    std::vector<BanEntry> out;
    out.reserve(bans.size());
    for (const auto& [id, entry] : bans) out.push_back(entry);
    std::sort(out.begin(), out.end(),
              [](const BanEntry& a, const BanEntry& b) { return a.nickname < b.nickname; });
    return out;
}

std::vector<std::string> GameServer::listOperatorNicknames() const
{
    std::vector<std::string> out;
    out.reserve(operators.size());
    for (const auto& [id, hint] : operators) out.push_back(hint);
    std::sort(out.begin(), out.end());
    return out;
}

std::optional<uint32_t> GameServer::findClientIdByNickname(const std::string& nickname) const
{
    for (const auto& [client_id, data] : clients)
    {
        if (data.identity.display_name == nickname) return client_id;
    }
    return std::nullopt;
}

std::optional<UUID> GameServer::findEntityByNickname(const std::string& nickname) const
{
    auto it = identity_to_entity.find(hashDisplayName(nickname));
    if (it == identity_to_entity.end()) return std::nullopt;
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

void GameServer::sendSystemMessageToAll(const std::wstring& message)
{
    ChatMessagePacket packet;
    packet.message = message + L'\n';
    auto buffer = serializePacket(packet);

    for (auto client_id : known_clients)
    {
        transport->send(client_id, buffer);
    }

    if (onChatBroadcast) onChatBroadcast(packet.message);
}


uint16_t GameServer::getTickRate() const
{
    return tick_rate;
}

float GameServer::getTickStep() const
{
    return 1.0f / static_cast<float>(tick_rate);
}

void GameServer::handleAdminRequest(uint32_t client_id, const std::string& nickname, const AdminRequestPacket& request)
{
    auto send_response = [&](AdminStatus status, std::vector<uint8_t> payload)
    {
        AdminResponsePacket response;
        response.request_id = request.request_id;
        response.op         = request.op;
        response.status     = status;
        response.payload    = std::move(payload);
        transport->send(client_id, serializePacket(response));
    };

    if (!isOperator(nickname))
    {
        std::cerr << "[Server] " << nickname << " attempted admin op "
                  << static_cast<int>(request.op) << " without operator rights\n";
        send_response(AdminStatus::Forbidden, {});
        return;
    }

    try
    {
        switch (request.op)
        {
            case AdminOp::ListEntities:
            {
                std::vector<uint8_t> payload;
                const auto& entities = world.getEntities();

                // Pre-collect so we can sort by label for a stable UI.
                struct Entry { UUID id; std::string label; };
                std::vector<Entry> entries;
                entries.reserve(entities.size());

                for (const auto& [id, entity] : entities)
                {
                    Entry entry;
                    entry.id = id;
                    if (entity.hasComponent<PlayerControlledComponent>())
                    {
                        entry.label = "Player: " + entity.getComponent<PlayerControlledComponent>().identity.display_name;
                    }
                    else
                    {
                        entry.label = std::to_string(entity.getComponents().size()) + " comps";
                    }
                    entries.push_back(std::move(entry));
                }

                std::sort(entries.begin(), entries.end(),
                          [](const Entry& a, const Entry& b) { return a.label < b.label; });

                writeUInt32(payload, static_cast<uint32_t>(entries.size()));
                for (const auto& entry : entries)
                {
                    payload.insert(payload.end(), entry.id.bytes.begin(), entry.id.bytes.end());
                    writeString(payload, entry.label);
                }

                std::cout << "[Server] " << nickname << " admin list ("
                          << entries.size() << " entities)\n";
                send_response(AdminStatus::Ok, std::move(payload));
                break;
            }
            case AdminOp::ReadEntity:
            {
                if (request.payload.size() < 16)
                {
                    send_response(AdminStatus::BadRequest, {});
                    break;
                }
                UUID id;
                std::copy_n(request.payload.begin(), id.bytes.size(), id.bytes.begin());

                if (!world.doesEntityExist(id))
                {
                    send_response(AdminStatus::NotFound, {});
                    break;
                }

                std::vector<uint8_t> payload;
                world.getEntity(id).serialize().save(payload, "");

                std::cout << "[Server] " << nickname << " admin read " << id.toString() << '\n';
                send_response(AdminStatus::Ok, std::move(payload));
                break;
            }
            case AdminOp::WriteEntity:
            {
                if (request.payload.size() < 16)
                {
                    send_response(AdminStatus::BadRequest, {});
                    break;
                }
                UUID id;
                std::copy_n(request.payload.begin(), id.bytes.size(), id.bytes.begin());

                if (!world.doesEntityExist(id))
                {
                    send_response(AdminStatus::NotFound, {});
                    break;
                }

                size_t pos = 16;
                Tag component_compound = Tag::load(request.payload, pos);

                auto& entity = world.getEntity(id);
                entity.applyUpdate(component_compound);

                if (component_compound.type() == TagType::T_COMPOUND)
                {
                    const TagCompound& compound = component_compound.get<TagCompound>();
                    for (auto& [type_index, component] : entity.getComponents())
                    {
                        if (compound.find(component->name()) != compound.end())
                        {
                            component->dirty = true;
                        }
                    }
                }

                std::cout << "[Server] " << nickname << " admin write " << id.toString() << '\n';
                send_response(AdminStatus::Ok, {});
                break;
            }
            case AdminOp::ListBans:
            {
                std::vector<BanEntry> entries = listBans();
                std::vector<uint8_t> payload;
                writeUInt32(payload, static_cast<uint32_t>(entries.size()));
                for (const auto& entry : entries)
                {
                    writeString(payload, entry.nickname);
                    writeString(payload, entry.reason);
                    writeString(payload, entry.banned_at);
                }
                std::cout << "[Server] " << nickname << " admin list bans ("
                          << entries.size() << ")\n";
                send_response(AdminStatus::Ok, std::move(payload));
                break;
            }
            case AdminOp::ListOperators:
            {
                std::vector<std::string> ops = listOperatorNicknames();
                std::vector<uint8_t> payload;
                writeUInt32(payload, static_cast<uint32_t>(ops.size()));
                for (const auto& op_name : ops)
                {
                    writeString(payload, op_name);
                }
                std::cout << "[Server] " << nickname << " admin list operators ("
                          << ops.size() << ")\n";
                send_response(AdminStatus::Ok, std::move(payload));
                break;
            }
            case AdminOp::AddBan:
            {
                size_t pos = 0;
                std::string target = readString(request.payload, pos);
                std::string reason = readString(request.payload, pos);
                if (target.empty())
                {
                    send_response(AdminStatus::BadRequest, {});
                    break;
                }
                addBan(target, reason);
                // Match /ban: kick if online so the ban takes effect immediately.
                kickClient(target, std::wstring(reason.begin(), reason.end()));
                std::cout << "[Server] " << nickname << " admin banned " << target;
                if (!reason.empty()) std::cout << " (reason: " << reason << ")";
                std::cout << '\n';
                send_response(AdminStatus::Ok, {});
                break;
            }
            case AdminOp::RemoveBan:
            {
                size_t pos = 0;
                std::string target = readString(request.payload, pos);
                if (target.empty())
                {
                    send_response(AdminStatus::BadRequest, {});
                    break;
                }
                removeBan(target);
                std::cout << "[Server] " << nickname << " admin unbanned " << target << '\n';
                send_response(AdminStatus::Ok, {});
                break;
            }
            case AdminOp::AddOperator:
            {
                size_t pos = 0;
                std::string target = readString(request.payload, pos);
                if (target.empty())
                {
                    send_response(AdminStatus::BadRequest, {});
                    break;
                }
                addOperator(target);
                std::cout << "[Server] " << nickname << " admin granted operator to " << target << '\n';
                send_response(AdminStatus::Ok, {});
                break;
            }
            case AdminOp::RemoveOperator:
            {
                size_t pos = 0;
                std::string target = readString(request.payload, pos);
                if (target.empty())
                {
                    send_response(AdminStatus::BadRequest, {});
                    break;
                }
                removeOperator(target);
                std::cout << "[Server] " << nickname << " admin revoked operator from " << target << '\n';
                send_response(AdminStatus::Ok, {});
                break;
            }
            default:
                send_response(AdminStatus::BadRequest, {});
                break;
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[Server] Admin op " << static_cast<int>(request.op)
                  << " from " << nickname << " threw: " << exception.what() << '\n';
        send_response(AdminStatus::Error, {});
    }
}