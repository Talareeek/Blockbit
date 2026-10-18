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
#include "../include/AnimationComponent.hpp"
#include "../include/AnimationSystem.hpp"
#include "../include/Prefab.hpp"
#include "../include/ChestView.hpp"
#include "../include/InventoryView.hpp"

#include <algorithm>
#include <iostream>

GameServer::GameServer(std::filesystem::path world_path, std::unique_ptr<ServerTransport> transport) : mode(ServerMode::Listen), transport(std::move(transport)), world(world_path)
{
    this->transport->start();
}

GameServer::GameServer(std::string name, unsigned int seed, GenerationProperties generation_properties, std::unique_ptr<ServerTransport> transport) : mode(ServerMode::Listen), transport(std::move(transport)), world(name, getWorldsPath() / name, seed, generation_properties)
{
    this->transport->start();
}

/*GameServer::GameServer(std::filesystem::path server_path, std::unique_ptr<ServerTransport> transport) : mode(ServerMode::Dedicated), transport(std::move(transport)), server_path(server_path)
{

}*/

void GameServer::sendChunkTo(uint32_t client_id, int around_chunk_position)
{
    constexpr int chunk_count = World::SIMULATION_DISTANCE + 1;
    int start = around_chunk_position - chunk_count / 2;

    auto& known_chunks = clients[client_id].known_chunks;

    for (int i = 0; i < chunk_count; i++)
    {
        int chunk_position = start + i;
        world.loadOrCreateChunk(chunk_position);

        ChunkPacket initialization;
        initialization.chunk = world.getChunk(chunk_position);
        transport->send(client_id, serializePacket(initialization));
        known_chunks.insert(chunk_position);
    }
}

void GameServer::streamChunksToClients()
{
    constexpr int half_distance = World::SIMULATION_DISTANCE / 2;

    for(auto& [client_id, data] : clients)
    {
        if(nickname_to_entity.contains(data.nickname))
        {
            Entity& entity = world.getEntity(nickname_to_entity[data.nickname]);

            if(!entity.hasComponent<TransformComponent>()) continue;

            double player_x = entity.getComponent<TransformComponent>().position.x;

            int player_chunk = (player_x >= 0.0f) ? static_cast<int>(player_x) / CHUNK_WIDTH : (static_cast<int>(player_x) - CHUNK_WIDTH + 1) / CHUNK_WIDTH;

            auto& known_chunks = data.known_chunks;
            auto& chunks = world.getChunks();

            for (int chunk_position = player_chunk - half_distance; chunk_position <= player_chunk + half_distance; ++chunk_position)
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

                    response.name = "Blockbit Server";
                    response.description = "A server for Blockbit game";

                    response.players = static_cast<uint32_t>(clients.size());
                    response.max_players = 20;

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
                case PacketType::Respawn:
                {
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

        AISystem(world, dt);
        ExplosiveSystem(world, dt);
        HealthSystem(world);
        PhysicsSystem(world, dt);
        PlayerControlledSystem(world, dt);
        InventorySystem(world);
        ChunkLoadSystem(world, dt);
        ChunkUnloadSystem(world);
        DaycycleSystem(world, dt);
        AnimationSystem(world, dt);

        TransformSystem(world);

        for(auto& [client_id, data] : clients)
        {
            auto it = nickname_to_entity.find(data.nickname);
            if (it != nickname_to_entity.end() && !world.doesEntityExist(it->second))
            {
                nickname_to_entity.erase(it);

                if (data.content_view)
                {
                    data.content_view->onClose(world);
                    data.content_view.reset();
                }

                transport->send(client_id, serializePacket(StopTrackingPacket{}));
                transport->send(client_id, serializePacket(CloseViewPacket{}));
            }
        }

        world.tick(dt);
        world.trySpawnPassiveMobs(dt);

        tick++;

        updatePlayerEntityConnections();

        dispatchPendingViewOpens();
        updateContentViews();

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
    }
    catch(const std::bad_alloc&)
    {
        std::cerr << "[Server] bad_alloc in tick\n";
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[Server] exception in tick: " << exception.what() << '\n';
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


uint16_t GameServer::getTickRate() const
{
    return tick_rate;
}

float GameServer::getTickStep() const
{
    return 1.0f / static_cast<float>(tick_rate);
}