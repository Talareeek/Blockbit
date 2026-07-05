#include "../include/GameServer.hpp"

#include "../include/Entity.hpp"
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

GameServer::GameServer(World& w, std::unique_ptr<ServerTransport> t, uint32_t hostId) : world(w), transport(std::move(t)), hostClientId(hostId)
{
    world.trackBlockChanges = true;

    transport->start();

    if (auto existing = world.findPlayerEntityByClient(hostClientId))
    {
        hostEntityId = *existing;
        std::cout << "[Server] Host client " << hostClientId << " reusing entity " << hostEntityId << '\n';
    }
    else
    {
        hostEntityId = world.spawnPlayer(hostClientId);
        std::cout << "[Server] Host client " << hostClientId << " spawned as entity " << hostEntityId << '\n';
    }

    if (hostClientId != 0)
    {
        clientToEntity[hostClientId] = hostEntityId;
        knownClients.insert(hostClientId);
        remoteInputQueues[hostClientId] = {};
    }
}

GameServer::~GameServer()
{
    transport->stop();
}

void GameServer::spawnRemotePlayer(uint32_t clientId, const std::string& nickname)
{
    uint32_t entityId;
    if (auto existing = world.findPlayerEntityByClient(clientId))
    {
        entityId = *existing;
        std::cout << "[Server] Client " << clientId << " logged in as \"" << nickname << "\", reusing entity " << entityId << '\n';
    }
    else
    {
        entityId = world.spawnPlayer(clientId);
        std::cout << "[Server] Client " << clientId << " logged in as \"" << nickname << "\", spawned as entity " << entityId << '\n';
    }

    for (auto& e : world.getEntities())
    {
        if (e.getID() == entityId && e.hasComponent<PlayerControlledComponent>())
        {
            e.getComponent<PlayerControlledComponent>().nickname = nickname;
            break;
        }
    }

    clientToEntity[clientId] = entityId;
    remoteInputQueues[clientId] = {};

    sf::Vector2f spawn = world.getSpawnPoint();
    int spawnChunk = static_cast<int>(spawn.x) / CHUNK_WIDTH;
    sendInitializationTo(clientId, spawnChunk);

    transport->send(clientId, serializePacket(SpawnPacket{entityId}));
}

void GameServer::despawnRemotePlayer(uint32_t clientId)
{
    auto it = clientToEntity.find(clientId);
    if (it == clientToEntity.end()) return;

    if (clientId == hostClientId)
    {
        clientToEntity.erase(it);
        remoteInputQueues.erase(clientId);
        std::cout << "[Server] Host client " << clientId << " left, host entity preserved\n";
        return;
    }

    uint32_t entityId = it->second;

    auto& entities = world.getEntities();
    entities.erase(std::remove_if(entities.begin(), entities.end(),
        [entityId](const Entity& e) { return e.getID() == entityId; }), entities.end());

    transport->broadcast(serializePacket(DespawnPacket{entityId}));

    clientToEntity.erase(it);
    remoteInputQueues.erase(clientId);
    sentChunks.erase(clientId);

    std::cout << "[Server] Client " << clientId << " left, despawned entity " << entityId << '\n';
}

void GameServer::sendInitializationTo(uint32_t clientId, int aroundChunkPos)
{
    constexpr int N = World::SIMULATION_DISTANCE + 1;
    int start = aroundChunkPos - N / 2;

    auto& sent = sentChunks[clientId];

    for (int i = 0; i < N; i++)
    {
        int cp = start + i;
        if (!world.getChunks().contains(cp))
        {
            world.generateChunk(cp);
        }

        InitializationPacket init;
        init.chunk = world.getChunk(cp);
        transport->send(clientId, serializePacket(init));
        sent.insert(cp);
    }
}

void GameServer::streamChunksToClients()
{
    constexpr int HALF = World::SIMULATION_DISTANCE / 2;

    for (auto& [clientId, entityId] : clientToEntity)
    {
        if (clientId == hostClientId) continue;

        Entity* playerEntity = nullptr;
        for (auto& e : world.getEntities())
        {
            if (e.getID() == entityId) { playerEntity = &e; break; }
        }
        if (!playerEntity || !playerEntity->hasComponent<TransformComponent>()) continue;

        float px = playerEntity->getComponent<TransformComponent>().position.x;
        int playerChunk = (px >= 0.0f)
            ? static_cast<int>(px) / CHUNK_WIDTH
            : (static_cast<int>(px) - CHUNK_WIDTH + 1) / CHUNK_WIDTH;

        auto& sent = sentChunks[clientId];
        auto& chunks = world.getChunks();

        for (int cp = playerChunk - HALF; cp <= playerChunk + HALF; ++cp)
        {
            if (sent.contains(cp)) continue;
            auto it = chunks.find(cp);
            if (it == chunks.end() || !it->second.generated) continue;

            InitializationPacket init;
            init.chunk = it->second;
            transport->send(clientId, serializePacket(init));
            sent.insert(cp);
        }
    }
}

void GameServer::syncConnections()
{
    auto current = transport->clientIds();
    std::unordered_set<uint32_t> currentSet(current.begin(), current.end());

    for (uint32_t id : current)
    {
        if (!knownClients.contains(id))
        {
            knownClients.insert(id);
            std::cout << "[Server] Client " << id << " connected, waiting for Login\n";
        }
    }

    std::vector<uint32_t> gone;
    for (uint32_t id : knownClients)
    {
        if (!currentSet.contains(id)) gone.push_back(id);
    }
    for (uint32_t id : gone)
    {
        despawnRemotePlayer(id);
        knownClients.erase(id);
    }
}

void GameServer::processIncoming()
{
    auto packets = transport->poll();
    for (auto& pkt : packets)
    {
        try
        {
            PacketReader r(pkt.payload.data(), pkt.payload.size());

            switch (pkt.type)
            {
                case PacketType::Login:
                {
                    LoginPacket login = deserializeLogin(r);

                    if (!knownClients.contains(pkt.clientId))
                    {
                        std::cerr << "[Server] Login from unknown client " << pkt.clientId << ", ignoring\n";
                        break;
                    }

                    auto it = clientToEntity.find(pkt.clientId);
                    if (it == clientToEntity.end())
                    {
                        spawnRemotePlayer(pkt.clientId, login.nickname);
                    }
                    else
                    {
                        uint32_t entityId = it->second;
                        for (auto& e : world.getEntities())
                        {
                            if (e.getID() == entityId && e.hasComponent<PlayerControlledComponent>())
                            {
                                e.getComponent<PlayerControlledComponent>().nickname = login.nickname;
                                break;
                            }
                        }
                        std::cout << "[Server] Client " << pkt.clientId << " re-logged as \"" << login.nickname << "\"\n";
                    }

                    break;
                }
                case PacketType::Input:
                {
                    InputPacket in = deserializeInput(r);
                    auto it = remoteInputQueues.find(pkt.clientId);
                    if (it == remoteInputQueues.end()) break;
                    it->second.push_back(std::move(in.inputs));
                    break;
                }
                case PacketType::BlockUpdate:
                {
                    BlockUpdatePacket bu = deserializeBlockUpdate(r);
                    world.setBlock(bu.x, bu.y, bu.block);
                    break;
                }
                case PacketType::StatusRequest:
                {
                    StatusResponsePacket response;

                    response.name = "Blockbit Server";
                    response.description = "A server for Blockbit game";

                    response.players = static_cast<uint32_t>(clientToEntity.size());
                    response.max_players = 20;

                    transport->send(pkt.clientId, serializePacket(response));

                    break;
                }
                default:
                    break;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Server] Bad packet from client " << pkt.clientId << ": " << e.what() << '\n';
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
        BlockUpdatePacket bu{x, y, block};
        if (hostClientId != 0)
            transport->broadcastExcept(hostClientId, serializePacket(bu));
        else
            transport->broadcast(serializePacket(bu));
    }
    world.pendingBlockUpdates.clear();
}

void GameServer::broadcastSnapshot()
{
    auto ids = transport->clientIds();
    if (ids.empty()) return;

    bool onlyHost = (hostClientId != 0) && (ids.size() == 1) && (ids[0] == hostClientId);
    if (onlyHost) return;

    SnapshotPacket snap;
    snap.dayTime = world.getDayTime();
    snap.days = 0;

    for (auto& e : world.getEntities())
    {
        if (!e.hasComponent<TransformComponent>() || !e.hasComponent<RenderComponent>()) continue;

        const auto& t = e.getComponent<TransformComponent>();
        const auto& r = e.getComponent<RenderComponent>();

        NetEntity ne{};
        ne.id        = e.getID();
        ne.x         = t.position.x;
        ne.y         = t.position.y;
        ne.size_x    = t.size.x;
        ne.size_y    = t.size.y;
        ne.textureID = r.textureID;
        ne.uv_x      = r.uv.position.x;
        ne.uv_y      = r.uv.position.y;
        ne.uv_size_x = r.uv.size.x;
        ne.uv_size_y = r.uv.size.y;

        if (e.hasComponent<HealthComponent>())
        {
            const auto& h = e.getComponent<HealthComponent>();
            ne.health    = h.health;
            ne.maxHealth = h.maxHealth;
        }

        if (e.hasComponent<InventoryComponent>())
        {
            const auto& inv = e.getComponent<InventoryComponent>();
            ne.inventory.reserve(inv.inventory.slots.size());
            for (const auto& slot : inv.inventory.slots)
            {
                ne.inventory.push_back({static_cast<uint32_t>(slot.itemID), slot.quantity});
            }
            ne.selectedSlot = inv.selectedSlot;
        }

        snap.entities.push_back(std::move(ne));
    }

    auto buf = serializePacket(snap);
    if (hostClientId != 0)
        transport->broadcastExcept(hostClientId, buf);
    else
        transport->broadcast(buf);
}

void GameServer::tick(float tick_step)
{
    try
    {
        syncConnections();
        processIncoming();

        for (auto& [clientId, queue] : remoteInputQueues)
        {
            auto it = clientToEntity.find(clientId);
            if (it == clientToEntity.end()) continue;
            if (queue.empty()) continue;

            processWorldInputs(world, std::move(queue.front()), it->second);
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
    catch (const std::exception& e)
    {
        std::cerr << "[Server] exception in tick: " << e.what() << '\n';
    }
}
