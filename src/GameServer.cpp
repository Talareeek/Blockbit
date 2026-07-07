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

#include "../include/AISystem.hpp"
#include "../include/TransformSystem.hpp"
#include "../include/ExplosiveSystem.hpp"
#include "../include/HealthSystem.hpp"
#include "../include/PhysicsSystem.hpp"
#include "../include/InventorySystem.hpp"
#include "../include/ChunkUnloadSystem.hpp"
#include "../include/DaycycleSystem.hpp"

#include <algorithm>
#include <iostream>

GameServer::GameServer(World world, std::unique_ptr<ServerTransport> transport, uint32_t host_client_id)
    : world(std::move(world)),
      transport(std::move(transport)),
      host_client_id(host_client_id)
{
    this->world.trackBlockChanges = true;

    this->transport->start();

    if (auto existing = this->world.findPlayerEntityByClient(host_client_id))
    {
        host_entity_id = *existing;
        std::cout << "[Server] Host client " << host_client_id << " reusing entity " << host_entity_id << '\n';
    }
    else
    {
        host_entity_id = this->world.spawnPlayer(host_client_id);
        std::cout << "[Server] Host client " << host_client_id << " spawned as entity " << host_entity_id << '\n';
    }

    if (host_client_id != 0)
    {
        client_to_entity[host_client_id] = host_entity_id;
        known_clients.insert(host_client_id);
        remote_input_queues[host_client_id] = {};
    }
}

GameServer::~GameServer()
{
    transport->stop();

    if (save_on_destruct)
    {
        try { world.save(); }
        catch (const std::exception& exception)
        {
            std::cerr << "[Server] world.save() failed: " << exception.what() << '\n';
        }
    }
}

void GameServer::spawnRemotePlayer(uint32_t client_id, const std::string& nickname)
{
    uint32_t entity_id;
    if (auto existing = world.findPlayerEntityByClient(client_id))
    {
        entity_id = *existing;
        std::cout << "[Server] Client " << client_id << " logged in as \"" << nickname << "\", reusing entity " << entity_id << '\n';
    }
    else
    {
        entity_id = world.spawnPlayer(client_id);
        std::cout << "[Server] Client " << client_id << " logged in as \"" << nickname << "\", spawned as entity " << entity_id << '\n';
    }

    for (auto& entity : world.getEntities())
    {
        if (entity.getID() == entity_id && entity.hasComponent<PlayerControlledComponent>())
        {
            entity.getComponent<PlayerControlledComponent>().nickname = nickname;
            break;
        }
    }

    client_to_entity[client_id] = entity_id;
    remote_input_queues[client_id] = {};

    sf::Vector2f spawn = world.getSpawnPoint();
    int spawn_chunk = static_cast<int>(spawn.x) / CHUNK_WIDTH;
    sendInitializationTo(client_id, spawn_chunk);

    transport->send(client_id, serializePacket(SpawnPacket{entity_id}));
}

void GameServer::despawnRemotePlayer(uint32_t client_id)
{
    auto iterator = client_to_entity.find(client_id);
    if (iterator == client_to_entity.end()) return;

    if (client_id == host_client_id)
    {
        client_to_entity.erase(iterator);
        remote_input_queues.erase(client_id);
        std::cout << "[Server] Host client " << client_id << " left, host entity preserved\n";
        return;
    }

    uint32_t entity_id = iterator->second;

    auto& entities = world.getEntities();
    entities.erase(std::remove_if(entities.begin(), entities.end(),
        [entity_id](const Entity& entity) { return entity.getID() == entity_id; }), entities.end());

    transport->broadcast(serializePacket(DespawnPacket{entity_id}));

    client_to_entity.erase(iterator);
    remote_input_queues.erase(client_id);
    sent_chunks.erase(client_id);

    std::cout << "[Server] Client " << client_id << " left, despawned entity " << entity_id << '\n';
}

void GameServer::sendInitializationTo(uint32_t client_id, int around_chunk_position)
{
    constexpr int chunk_count = World::SIMULATION_DISTANCE + 1;
    int start = around_chunk_position - chunk_count / 2;

    auto& sent = sent_chunks[client_id];

    for (int i = 0; i < chunk_count; i++)
    {
        int chunk_position = start + i;
        if (!world.getChunks().contains(chunk_position))
        {
            world.generateChunk(chunk_position);
        }

        InitializationPacket initialization;
        initialization.chunk = world.getChunk(chunk_position);
        transport->send(client_id, serializePacket(initialization));
        sent.insert(chunk_position);
    }
}

void GameServer::streamChunksToClients()
{
    constexpr int half_distance = World::SIMULATION_DISTANCE / 2;

    for (auto& [client_id, entity_id] : client_to_entity)
    {
        Entity* player_entity = nullptr;
        for (auto& entity : world.getEntities())
        {
            if (entity.getID() == entity_id) { player_entity = &entity; break; }
        }
        if (!player_entity || !player_entity->hasComponent<TransformComponent>()) continue;

        float player_x = player_entity->getComponent<TransformComponent>().position.x;
        int player_chunk = (player_x >= 0.0f)
            ? static_cast<int>(player_x) / CHUNK_WIDTH
            : (static_cast<int>(player_x) - CHUNK_WIDTH + 1) / CHUNK_WIDTH;

        auto& sent = sent_chunks[client_id];
        auto& chunks = world.getChunks();

        for (int chunk_position = player_chunk - half_distance; chunk_position <= player_chunk + half_distance; ++chunk_position)
        {
            if (sent.contains(chunk_position)) continue;
            auto iterator = chunks.find(chunk_position);
            if (iterator == chunks.end() || !iterator->second.generated) continue;

            InitializationPacket initialization;
            initialization.chunk = iterator->second;
            transport->send(client_id, serializePacket(initialization));
            sent.insert(chunk_position);
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
        despawnRemotePlayer(client_id);
        known_clients.erase(client_id);
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

                    if (!known_clients.contains(packet.clientId))
                    {
                        std::cerr << "[Server] Login from unknown client " << packet.clientId << ", ignoring\n";
                        break;
                    }

                    auto iterator = client_to_entity.find(packet.clientId);
                    if (iterator == client_to_entity.end())
                    {
                        spawnRemotePlayer(packet.clientId, login.nickname);
                    }
                    else
                    {
                        uint32_t entity_id = iterator->second;
                        for (auto& entity : world.getEntities())
                        {
                            if (entity.getID() == entity_id && entity.hasComponent<PlayerControlledComponent>())
                            {
                                entity.getComponent<PlayerControlledComponent>().nickname = login.nickname;
                                break;
                            }
                        }
                        std::cout << "[Server] Client " << packet.clientId << " re-logged as \"" << login.nickname << "\"\n";

                        sf::Vector2f spawn = world.getSpawnPoint();
                        int spawn_chunk = static_cast<int>(spawn.x) / CHUNK_WIDTH;
                        sendInitializationTo(packet.clientId, spawn_chunk);
                        transport->send(packet.clientId, serializePacket(SpawnPacket{entity_id}));
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
                    world.setBlock(block_update.x, block_update.y, block_update.block);
                    break;
                }
                case PacketType::StatusRequest:
                {
                    StatusResponsePacket response;

                    response.name = "Blockbit Server";
                    response.description = "A server for Blockbit game";

                    response.players = static_cast<uint32_t>(client_to_entity.size());
                    response.max_players = 20;

                    transport->send(packet.clientId, serializePacket(response));

                    break;
                }
                case PacketType::ChatMessage:
                {
                    ChatMessagePacket chat_message = deserializeChatMessage(reader);

                    std::string nickname = entityWithID(client_to_entity[packet.clientId], world).getComponent<PlayerControlledComponent>().nickname;
                    std::wstring wide_nickname(nickname.begin(), nickname.end());

                    sendChat(wide_nickname, chat_message.message);

                    break;
                }
                default:
                    break;
            }
        }
        catch (const std::exception& exception)
        {
            std::cerr << "[Server] Bad packet from client " << packet.clientId << ": " << exception.what() << '\n';
        }
    }
}

void GameServer::runSystems(float tick_step)
{
    auto& entities = world.getEntities();

    AISystem(world, tick_step);
    TransformSystem(world);
    ExplosiveSystem(world, tick_step);
    HealthSystem(world);
    PhysicsSystem(entities, world, tick_step);
    InventorySystem(entities);
    ChunkUnloadSystem(world);
    DaycycleSystem(world, tick_step);

    world.tick(tick_step);
}

void GameServer::broadcastBlockUpdates()
{
    if (world.pendingBlockUpdates.empty()) return;

    for (auto& [x, y, block] : world.pendingBlockUpdates)
    {
        BlockUpdatePacket block_update{x, y, block};
        transport->broadcast(serializePacket(block_update));
    }
    world.pendingBlockUpdates.clear();
}

void GameServer::broadcastSnapshot()
{
    auto ids = transport->clientIds();
    if (ids.empty()) return;

    SnapshotPacket snapshot;
    snapshot.dayTime = world.getDayTime();
    snapshot.days = 0;

    for (auto& entity : world.getEntities())
    {
        if (!entity.hasComponent<TransformComponent>() || !entity.hasComponent<RenderComponent>()) continue;

        const auto& transform = entity.getComponent<TransformComponent>();
        const auto& render = entity.getComponent<RenderComponent>();

        NetEntity net_entity{};
        net_entity.id        = entity.getID();
        net_entity.x         = transform.position.x;
        net_entity.y         = transform.position.y;
        net_entity.size_x    = transform.size.x;
        net_entity.size_y    = transform.size.y;
        net_entity.textureID = render.textureID;
        net_entity.uv_x      = render.uv.position.x;
        net_entity.uv_y      = render.uv.position.y;
        net_entity.uv_size_x = render.uv.size.x;
        net_entity.uv_size_y = render.uv.size.y;

        if (entity.hasComponent<HealthComponent>())
        {
            const auto& health = entity.getComponent<HealthComponent>();
            net_entity.health    = health.health;
            net_entity.maxHealth = health.maxHealth;
        }

        if (entity.hasComponent<InventoryComponent>())
        {
            const auto& inventory = entity.getComponent<InventoryComponent>();
            net_entity.inventory.reserve(inventory.inventory.slots.size());
            for (const auto& slot : inventory.inventory.slots)
            {
                net_entity.inventory.push_back({static_cast<uint32_t>(slot.itemID), slot.quantity});
            }
            net_entity.selectedSlot = inventory.selectedSlot;
        }

        snapshot.entities.push_back(std::move(net_entity));
    }

    auto buffer = serializePacket(snapshot);
    transport->broadcast(buffer);
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

void GameServer::tick(float tick_step)
{
    try
    {
        syncConnections();
        processIncoming();

        for (auto& [client_id, queue] : remote_input_queues)
        {
            auto iterator = client_to_entity.find(client_id);
            if (iterator == client_to_entity.end()) continue;
            if (queue.empty()) continue;

            processWorldInputs(world, std::move(queue.front()), iterator->second);
            queue.pop_front();
        }

        runSystems(tick_step);

        streamChunksToClients();
        broadcastBlockUpdates();
        broadcastSnapshot();
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "[Server] bad_alloc in tick\n";
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[Server] exception in tick: " << exception.what() << '\n';
    }
}
