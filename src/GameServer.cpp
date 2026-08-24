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

#include <algorithm>
#include <iostream>

GameServer::GameServer(std::filesystem::path world_path, std::unique_ptr<ServerTransport> transport) : mode(ServerMode::Listen), transport(std::move(transport)), world(world_path)
{
    this->world.trackBlockChanges = true;

    this->transport->start();
}

GameServer::GameServer(std::string name, unsigned int seed, GenerationProperties generation_properties, std::unique_ptr<ServerTransport> transport) : mode(ServerMode::Listen), transport(std::move(transport)), world(name, getWorldsPath() / name, seed, generation_properties)
{
    this->world.trackBlockChanges = true;

    this->transport->start();
}

/*GameServer::GameServer(std::filesystem::path server_path, std::unique_ptr<ServerTransport> transport) : mode(ServerMode::Dedicated), transport(std::move(transport)), server_path(server_path)
{

}*/

void GameServer::sendChunkTo(uint32_t client_id, int around_chunk_position)
{
    constexpr int chunk_count = World::SIMULATION_DISTANCE + 1;
    int start = around_chunk_position - chunk_count / 2;

    auto& sent = sent_chunks[client_id];

    for (int i = 0; i < chunk_count; i++)
    {
        int chunk_position = start + i;
        world.loadOrCreateChunk(chunk_position);

        ChunkPacket initialization;
        initialization.chunk = world.getChunk(chunk_position);
        transport->send(client_id, serializePacket(initialization));
        sent.insert(chunk_position);
    }
}

void GameServer::streamChunksToClients()
{
    constexpr int half_distance = World::SIMULATION_DISTANCE / 2;

    for(auto& [client_id, nickname] : client_to_nickname)
    {
        if(nickname_to_entity.contains(nickname))
        {
            Entity& entity = world.getEntity(nickname_to_entity[nickname]);

            if(!entity.hasComponent<TransformComponent>()) continue;

            double player_x = entity.getComponent<TransformComponent>().position.x;

            int player_chunk = (player_x >= 0.0f) ? static_cast<int>(player_x) / CHUNK_WIDTH : (static_cast<int>(player_x) - CHUNK_WIDTH + 1) / CHUNK_WIDTH;

            auto& sent = sent_chunks[client_id];
            auto& chunks = world.getChunks();

            for (int chunk_position = player_chunk - half_distance; chunk_position <= player_chunk + half_distance; ++chunk_position)
            {
                if (sent.contains(chunk_position)) continue;
                auto iterator = chunks.find(chunk_position);
                if (iterator == chunks.end() || !iterator->second.generated) continue;

                ChunkPacket initialization;
                initialization.chunk = iterator->second;
                transport->send(client_id, serializePacket(initialization));
                sent.insert(chunk_position);
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
        deactivatePlayerFor(client_to_nickname[client_id]);
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

                    if(!client_to_nickname.contains(packet.clientId))
                    {
                        client_to_nickname[packet.clientId] = login.nickname;
                        remote_input_queues[packet.clientId] = {};
                        spawnPlayerFor(login.nickname);

                        UUID entity_id = nickname_to_entity[login.nickname];
                        transport->send(packet.clientId, serializePacket(SpawnPacket{entity_id}));

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
                    world.setBlock(block_update.x, block_update.y, block_update.block);
                    break;
                }
                case PacketType::StatusRequest:
                {
                    StatusResponsePacket response;

                    response.name = "Blockbit Server";
                    response.description = "A server for Blockbit game";

                    response.players = static_cast<uint32_t>(client_to_nickname.size());
                    response.max_players = 20;

                    transport->send(packet.clientId, serializePacket(response));

                    break;
                }
                case PacketType::ChatMessage:
                {
                    ChatMessagePacket chat_message = deserializeChatMessage(reader);

                    std::string nickname = client_to_nickname[packet.clientId];
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

                    world.getEntity(nickname_to_entity[client_to_nickname[packet.clientId]]).getComponent<PlayerControlledComponent>().cursor_position = {client_snapshot.cursor_x, client_snapshot.cursor_y};
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

    snapshot.tick = tick;

    snapshot.dayTime = world.getDayTime();
    snapshot.days = 0;

    for (auto& [id, entity] : world.getEntities())
    {
        if (!entity.hasComponent<TransformComponent>() || !entity.hasComponent<RenderComponent>()) continue;

        const auto& transform = entity.getComponent<TransformComponent>();
        const auto& render = entity.getComponent<RenderComponent>();

        NetEntity net_entity{};
        net_entity.id        = id;
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


void GameServer::update(float dt)
{
    try
    {
        syncConnections();
        processIncoming();

        for (auto& [client_id, queue] : remote_input_queues)
        {
            if(!client_to_nickname.contains(client_id)) continue;

            std::string nickname = client_to_nickname[client_id];

            auto iterator = nickname_to_entity.find(nickname);
            if (iterator == nickname_to_entity.end()) continue;
            if (queue.empty()) continue;

            processWorldInputs(world, std::move(queue.front()), iterator->second);
            queue.pop_front();
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

        TransformSystem(world);

        world.tick(dt);

        tick++;

        updatePlayerEntityConnections();

        streamChunksToClients();
        broadcastBlockUpdates();
        broadcastSnapshot();
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
        Entity entity(generateUUID());

        entity.addComponent(TransformComponent{{0.0f, 0.0f}, {1.0f, 1.0f}, sf::degrees(0.0f)});
        entity.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});

        InventoryComponent inv(36);
        inv.inventory.slots[0] = {ItemID::Dynamite, 16};
        inv.inventory.slots[1] = {ItemID::Bucket, 1};
        inv.inventory.slots[2] = {ItemID::Woodcutter, 64};
        inv.inventory.slots[3] = {ItemID::Lighter, 1};
        inv.inventory.slots[4] = {ItemID::Diamond_Pickaxe, 1};
        inv.inventory.slots[5] = {ItemID::Diamond_Axe, 1};
        inv.inventory.slots[6] = {ItemID::Diamond_Shovel, 1};
        entity.addComponent(std::move(inv));

        entity.getComponent<TransformComponent>().position = world.getSpawnPoint();

        entity.addComponent(RenderComponent{0, {{0, 0}, {16, 16}}, {1.0f, 1.0f}});
        entity.addComponent(HealthComponent{100, 100, false});
        entity.addComponent(PlayerControlledComponent{nickname});

        nickname_to_entity[nickname] = entity.getID();

        world.addEntity(std::move(entity));
    }
}

void GameServer::deactivatePlayerFor(std::string nickname)
{
    if(!nickname_to_entity.contains(nickname)) return;

    world.savePlayer(nickname_to_entity[nickname]);

    world.removeEntity(nickname_to_entity[nickname]);

    nickname_to_entity.erase(nickname);
}

void GameServer::updatePlayerEntityConnections()
{
    for(auto& [nickname, entity_id] : nickname_to_entity)
    {
        if(!world.doesEntityExist(entity_id)) nickname_to_entity.erase(nickname);
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