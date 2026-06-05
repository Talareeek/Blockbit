#include "../include/HostGameState.hpp"
#include "../include/Game.hpp"
#include "../include/Entity.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/Chunk.hpp"
#include "../include/Render.hpp"

#include <iostream>

HostGameState::HostGameState(Game* game, World world, uint16_t port)
    : MainGameState(game, std::move(world)), server(port)
{
    this->world.trackBlockChanges = true;
    server.start();
    std::cout << "[Host] Server started on port " << port << '\n';
}

HostGameState::~HostGameState()
{
    server.stop();
}

void HostGameState::spawnRemotePlayer(uint32_t clientId)
{
    Entity remote(world.getPossibleID());

    sf::Vector2f spawn = world.getSpawnPoint();

    remote.addComponent(TransformComponent{sf::Vector2<double>(spawn), {1.0, 1.0}, sf::degrees(0.0f)});
    remote.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});
    remote.addComponent(RenderComponent{0, {{0, 0}, {16, 16}}, {1.0f, 1.0f}});
    remote.addComponent(HealthComponent{100, 100});
    remote.addComponent(InventoryComponent(36));

    uint32_t entityId = remote.getID();
    world.getEntities().push_back(std::move(remote));

    clientToEntity[clientId] = entityId;
    remoteInputQueues[clientId] = {};

    std::cout << "[Host] Client " << clientId << " joined, spawned as entity " << entityId << '\n';

    int spawnChunk = static_cast<int>(spawn.x) / CHUNK_WIDTH;
    sendInitializationTo(clientId, spawnChunk);

    server.send(clientId, serializePacket(SpawnPacket{entityId}));
}

void HostGameState::despawnRemotePlayer(uint32_t clientId)
{
    auto it = clientToEntity.find(clientId);
    if (it == clientToEntity.end()) return;

    uint32_t entityId = it->second;

    auto& entities = world.getEntities();
    entities.erase(std::remove_if(entities.begin(), entities.end(),
        [entityId](const Entity& e) { return e.getID() == entityId; }), entities.end());

    server.broadcast(serializePacket(DespawnPacket{entityId}));

    clientToEntity.erase(it);
    remoteInputQueues.erase(clientId);

    std::cout << "[Host] Client " << clientId << " left, despawned entity " << entityId << '\n';
}

void HostGameState::sendInitializationTo(uint32_t clientId, int aroundChunkPos)
{
    constexpr int N = World::SIMULATION_DISTANCE + 1;
    int start = aroundChunkPos - N / 2;

    for (int i = 0; i < N; i++)
    {
        int cp = start + i;
        if (!world.getChunks().contains(cp))
        {
            world.generateChunk(cp);
        }

        InitializationPacket init;
        init.chunk = world.getChunk(cp);
        server.send(clientId, serializePacket(init));
    }
}

void HostGameState::syncConnections()
{
    auto current = server.clientIds();
    std::unordered_set<uint32_t> currentSet(current.begin(), current.end());

    for (uint32_t id : current)
    {
        if (!knownClients.contains(id))
        {
            spawnRemotePlayer(id);
            knownClients.insert(id);
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

void HostGameState::processIncoming()
{
    auto packets = server.poll();
    for (auto& pkt : packets)
    {
        try
        {
            PacketReader r(pkt.payload.data(), pkt.payload.size());

            switch (pkt.type)
            {
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
                default:
                    break;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Host] Bad packet from client " << pkt.clientId << ": " << e.what() << '\n';
        }
    }
}

void HostGameState::onTick(float /*tick_step*/)
{
    for (auto& [clientId, queue] : remoteInputQueues)
    {
        auto it = clientToEntity.find(clientId);
        if (it == clientToEntity.end()) continue;
        if (queue.empty()) continue;

        processInputs(std::move(queue.front()), it->second);
        queue.pop_front();
    }

    broadcastSnapshot();
}

void HostGameState::broadcastBlockUpdates()
{
    if (world.pendingBlockUpdates.empty()) return;

    for (auto& [x, y, block] : world.pendingBlockUpdates)
    {
        BlockUpdatePacket bu{x, y, block};
        server.broadcast(serializePacket(bu));
    }
    world.pendingBlockUpdates.clear();
}

void HostGameState::broadcastSnapshot()
{
    if (server.clientIds().empty()) return;

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

        snap.entities.push_back(std::move(ne));
    }

    server.broadcast(serializePacket(snap));
}

void HostGameState::handleEvent(const sf::Event& event)
{
    MainGameState::handleEvent(event);
}

void HostGameState::update(float dt)
{
    try
    {
        syncConnections();
        processIncoming();

        MainGameState::update(dt);

        broadcastBlockUpdates();
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "[Host] bad_alloc in update\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Host] exception in update: " << e.what() << '\n';
    }
}
