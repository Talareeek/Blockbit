#include "../include/ClientGameState.hpp"
#include "../include/Game.hpp"
#include "../include/Entity.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/RenderSystem.hpp"
#include "../include/AssetManager.hpp"
#include "../include/Chunk.hpp"

#include <iostream>
#include <algorithm>

ClientGameState::ClientGameState(Game* game, const std::string& host, uint16_t port)
    : GameState(game)
{
    if (!client.connect(host, port))
    {
        std::cerr << "[Client] Failed to connect to " << host << ':' << port << '\n';
        connectionFailed = true;
        return;
    }
    std::cout << "[Client] Connected to " << host << ':' << port << '\n';
}

ClientGameState::~ClientGameState()
{
    client.disconnect();
}

void ClientGameState::rebuildEntitiesFromSnapshot(const SnapshotPacket& snap)
{
    auto& entities = world.getEntities();
    entities.clear();
    entities.reserve(snap.entities.size());

    for (const auto& ne : snap.entities)
    {
        Entity e(ne.id);

        e.addComponent(TransformComponent{
            {ne.x, ne.y},
            {ne.size_x, ne.size_y},
            sf::degrees(0.0f)
        });
        e.addComponent(RenderComponent{
            static_cast<uint16_t>(ne.textureID),
            sf::IntRect{{ne.uv_x, ne.uv_y}, {ne.uv_size_x, ne.uv_size_y}},
            {ne.size_x, ne.size_y}
        });
        e.addComponent(HealthComponent{ne.health, ne.maxHealth, false});

        entities.push_back(std::move(e));
    }
}

void ClientGameState::processIncoming()
{
    auto packets = client.poll();
    for (auto& pkt : packets)
    {
        try
        {
            PacketReader r(pkt.payload.data(), pkt.payload.size());

            switch (pkt.type)
            {
                case PacketType::Initialization:
                {
                    auto init = deserializeInitialization(r);
                    for (auto& chunk : init.chunks)
                    {
                        world.getChunks()[chunk.chunk_position] = chunk;
                        world.chunkMeshes.erase(chunk.chunk_position);
                    }
                    initialized = true;
                    break;
                }
                case PacketType::Snapshot:
                {
                    auto snap = deserializeSnapshot(r);
                    rebuildEntitiesFromSnapshot(snap);
                    break;
                }
                case PacketType::BlockUpdate:
                {
                    auto bu = deserializeBlockUpdate(r);
                    world.setBlock(bu.x, bu.y, bu.block);
                    int chunkPos = (bu.x >= 0) ? bu.x / CHUNK_WIDTH : (bu.x - CHUNK_WIDTH + 1) / CHUNK_WIDTH;
                    world.chunkMeshes.erase(chunkPos);
                    break;
                }
                case PacketType::Spawn:
                {
                    auto sp = deserializeSpawn(r);
                    myEntityId = sp.id;
                    world.setPlayerID(myEntityId);
                    break;
                }
                case PacketType::Despawn:
                {
                    auto dp = deserializeDespawn(r);
                    auto& entities = world.getEntities();
                    entities.erase(std::remove_if(entities.begin(), entities.end(),
                        [&](const Entity& e) { return e.getID() == dp.id; }), entities.end());
                    break;
                }
                case PacketType::Input:
                    break;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Client] Bad packet: " << e.what() << '\n';
        }
    }
}

void ClientGameState::sendInput()
{
    if (!client.isConnected() || myEntityId == 0) return;

    InputPacket in{};
    in.id    = myEntityId;
    in.left  = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A);
    in.right = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D);
    in.jump  = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);

    if (in.left != lastSent.left || in.right != lastSent.right || in.jump != lastSent.jump)
    {
        client.send(serializePacket(in));
        lastSent = in;
    }
}

void ClientGameState::handleEvent(const sf::Event& event)
{
    if (event.is<sf::Event::Resized>())
    {
        world.chunkMeshes.clear();
    }
}

void ClientGameState::update(float dt)
{
    if (connectionFailed || !client.isConnected())
    {
        game->popState();
        return;
    }

    processIncoming();

    sendTimer += dt;
    if (sendTimer >= INPUT_INTERVAL)
    {
        sendTimer = 0.0f;
        sendInput();
    }
}

void ClientGameState::render(sf::RenderWindow& window)
{
    sf::RectangleShape sky({static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)});
    sky.setPosition({0.0f, 0.0f});
    sky.setFillColor(world.getSkyColor(world.getDayTime() / World::DAY_CYCLE_DURATION));
    window.draw(sky);

    if (!initialized)
    {
        sf::Text waiting(AssetManager::getFont(0), "Connecting...", 30);
        waiting.setPosition({window.getSize().x * 0.5f - 100.0f, window.getSize().y * 0.5f});
        waiting.setFillColor(sf::Color::White);
        window.draw(waiting);
        return;
    }

    auto& entities = world.getEntities();
    unsigned int unit_size = window.getSize().y / UNIT_SIZE_FACTOR;

    sf::Vector2f camPos{0.0f, 0.0f};
    if (myEntityId != 0)
    {
        auto it = std::find_if(entities.begin(), entities.end(),
            [this](const Entity& e) { return e.getID() == myEntityId; });
        if (it != entities.end() && it->hasComponent<TransformComponent>())
        {
            camPos = it->getComponent<TransformComponent>().position;
        }
    }

    sf::View view(
        {(camPos.x + 0.5f) * unit_size, (camPos.y - 0.5f) * unit_size},
        {(float)window.getSize().x, (float)window.getSize().y}
    );
    view.setSize({view.getSize().x, -view.getSize().y});
    window.setView(view);

    RenderSystem(entities, window);

    RenderWorld(world, window);

    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));
}
