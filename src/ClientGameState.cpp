#include "../include/ClientGameState.hpp"
#include "../include/Game.hpp"
#include "../include/Entity.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/RenderSystem.hpp"
#include "../include/AssetManager.hpp"
#include "../include/Chunk.hpp"
#include "../include/Render.hpp"

#include <iostream>
#include <algorithm>

ClientGameState::ClientGameState(Game* game, const std::string& host, uint16_t port)
    : GameState(game)
{
    pendingHost = host;
    pendingPort = port;
    remoteAddress = host + ":" + std::to_string(port);
    std::cerr << "[Client] Will attempt connection to " << remoteAddress << '\n';
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
    std::vector<ReceivedPacket> packets;
    try { packets = client.poll(); }
    catch (const std::bad_alloc&) { errorMessage = "Out of memory"; connectionFailed = true; return; }

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
                    world.getChunks()[init.chunk.chunk_position] = init.chunk;
                    world.chunkMeshes.erase(init.chunk.chunk_position);
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
        catch (const std::bad_alloc&)
        {
            std::cerr << "[Client] bad_alloc decoding packet (type " << static_cast<int>(pkt.type) << ", size " << pkt.payload.size() << ")\n";
            errorMessage = "Out of memory";
            connectionFailed = true;
            return;
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

    if (event.is<sf::Event::KeyPressed>())
    {
        auto key = event.getIf<sf::Event::KeyPressed>();
        if (key->code == sf::Keyboard::Key::Escape && (connectionFailed || !client.isConnected()))
        {
            game->popState();
            return;
        }
    }
}

void ClientGameState::update(float dt)
{
    try
    {
        if (!connectAttempted && !connectionFailed)
        {
            connectAttempted = true;
            if (!client.connect(pendingHost, pendingPort, std::chrono::seconds(3)))
            {
                connectionFailed = true;
                errorMessage = client.getLastError();
                if (errorMessage.empty()) errorMessage = "Connection failed";
                std::cerr << "[Client] connect failed: " << errorMessage << '\n';
                return;
            }
            wasConnected = true;
            std::cerr << "[Client] connected to " << remoteAddress << '\n';
        }

        if (!connectionFailed && wasConnected && !client.isConnected())
        {
            connectionFailed = true;
            if (errorMessage.empty())
            {
                errorMessage = client.getLastError();
                if (errorMessage.empty()) errorMessage = "Disconnected from server";
            }
        }

        if (connectionFailed) return;

        processIncoming();

        sendTimer += dt;
        if (sendTimer >= INPUT_INTERVAL)
        {
            sendTimer = 0.0f;
            sendInput();
        }
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "[Client] bad_alloc caught in update\n";
        connectionFailed = true;
        errorMessage = "Out of memory while updating";
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Client] exception in update: " << e.what() << '\n';
        connectionFailed = true;
        errorMessage = e.what();
    }
}

void ClientGameState::render(sf::RenderWindow& window)
{
    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));

    if (connectionFailed)
    {
        sf::RectangleShape bg({static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)});
        bg.setFillColor(sf::Color(18, 22, 30));
        window.draw(bg);

        std::string title = wasConnected ? "Disconnected" : "Cannot connect to server";
        std::string detail = remoteAddress.empty() ? "" : remoteAddress;
        std::string reason = errorMessage.empty() ? "" : ("Reason: " + errorMessage);
        std::string hint = "Press ESC to go back";

        sf::Text titleText(AssetManager::getFont(0), title, 36);
        titleText.setFillColor(sf::Color(230, 80, 80));
        titleText.setOutlineColor(sf::Color::Black);
        titleText.setOutlineThickness(2.0f);
        auto tb = titleText.getLocalBounds();
        titleText.setPosition({(window.getSize().x - tb.size.x) * 0.5f, window.getSize().y * 0.35f});
        window.draw(titleText);

        if (!detail.empty())
        {
            sf::Text detailText(AssetManager::getFont(0), detail, 22);
            detailText.setFillColor(sf::Color(220, 220, 220));
            auto db = detailText.getLocalBounds();
            detailText.setPosition({(window.getSize().x - db.size.x) * 0.5f, window.getSize().y * 0.35f + 50.0f});
            window.draw(detailText);
        }

        if (!reason.empty())
        {
            sf::Text reasonText(AssetManager::getFont(0), reason, 20);
            reasonText.setFillColor(sf::Color(200, 200, 200));
            auto rb = reasonText.getLocalBounds();
            reasonText.setPosition({(window.getSize().x - rb.size.x) * 0.5f, window.getSize().y * 0.35f + 85.0f});
            window.draw(reasonText);
        }

        sf::Text hintText(AssetManager::getFont(0), hint, 18);
        hintText.setFillColor(sf::Color(180, 180, 180));
        auto hb = hintText.getLocalBounds();
        hintText.setPosition({(window.getSize().x - hb.size.x) * 0.5f, window.getSize().y * 0.55f});
        window.draw(hintText);
        return;
    }

    sf::RectangleShape sky({static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)});
    sky.setPosition({0.0f, 0.0f});
    sky.setFillColor(world.getSkyColor(world.getDayTime() / World::DAY_CYCLE_DURATION));
    window.draw(sky);

    if (!initialized)
    {
        sf::Text waiting(AssetManager::getFont(0), "Connecting to " + remoteAddress + "...", 28);
        waiting.setFillColor(sf::Color::White);
        waiting.setOutlineColor(sf::Color::Black);
        waiting.setOutlineThickness(2.0f);
        auto wb = waiting.getLocalBounds();
        waiting.setPosition({(window.getSize().x - wb.size.x) * 0.5f, window.getSize().y * 0.5f});
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
            camPos = sf::Vector2f(it->getComponent<TransformComponent>().position);
        }
    }

    sf::View view(
        {(camPos.x + 0.5f) * unit_size, (camPos.y - 0.5f) * unit_size},
        {(float)window.getSize().x, (float)window.getSize().y}
    );
    view.setSize({view.getSize().x, -view.getSize().y});
    window.setView(view);

    try
    {
        RenderSystem(entities, window);
        RenderWorld(world, window);
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "[Client] bad_alloc in render\n";
        connectionFailed = true;
        errorMessage = "Out of memory while rendering";
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Client] exception in render: " << e.what() << '\n';
        connectionFailed = true;
        errorMessage = e.what();
    }

    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));
}
