#include "../include/ClientGameState.hpp"
#include "../include/Game.hpp"
#include "../include/Entity.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/AssetManager.hpp"
#include "../include/Chunk.hpp"
#include "../include/NetworkClientTransport.hpp"
#include "../include/NetworkServerTransport.hpp"
#include "../include/LoopbackServerTransport.hpp"

#include <iostream>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdlib>

ClientGameState::ClientGameState(Game* game, const std::string& host, uint16_t port, std::string nickname) : MainGameState(game, World{})
{
    saveOnDestruct = false;

    game->getWindow().setTitle("Blockbit - Multiplayer");

    pendingHost = host;
    pendingPort = port;
    remoteAddress = host + ":" + std::to_string(port);
    this->nickname = std::move(nickname);

    transport = std::make_unique<NetworkClientTransport>();

    game->getConsole().writeLine(L"[Client] Will attempt connection to " + std::wstring(remoteAddress.begin(), remoteAddress.end()));
}

ClientGameState::ClientGameState(Game* game, World world, uint16_t networkPort, std::string nickname) : MainGameState(game, std::move(world))
{
    this->nickname = std::move(nickname);

    if (networkPort == 0)
    {
        game->getWindow().setTitle("Blockbit - Singleplayer");

        auto pair = makeLoopbackPair();
        localServer.emplace(this->world, std::move(pair.serverSide), LoopbackChannel::LOOPBACK_CLIENT_ID);
        transport = std::move(pair.clientSide);
        transport->connect("loopback", 0);
        remoteAddress = "local";

        transport->send(serializePacket(LoginPacket{this->nickname}));
        loginSent = true;
    }
    else
    {
        game->getWindow().setTitle("Blockbit - Host");

        localServer.emplace(this->world, std::make_unique<NetworkServerTransport>(networkPort), 0u);
        remoteAddress = "0.0.0.0:" + std::to_string(networkPort);
        std::cerr << "[Server] Listening on port " << networkPort << '\n';
    }

    myEntityId = localServer->getHostEntityId();
    localPlayerEntityId = myEntityId;
    initialized = true;
    wasConnected = true;
    connectAttempted = true;
}

ClientGameState::~ClientGameState()
{
    if (transport) transport->disconnect();
}

void ClientGameState::rebuildEntitiesFromSnapshot(const SnapshotPacket& snap)
{
    auto& entities = world.getEntities();

    entities.clear();
    entities.reserve(snap.entities.size());

    for (const auto& net_entity : snap.entities)
    {
        Entity entity(net_entity.id);

        entity.addComponent(TransformComponent{{net_entity.x, net_entity.y}, {net_entity.size_x, net_entity.size_y}, sf::degrees(0.0f)});
        entity.addComponent(RenderComponent{static_cast<uint16_t>(net_entity.textureID), sf::IntRect{{net_entity.uv_x, net_entity.uv_y}, {net_entity.uv_size_x, net_entity.uv_size_y}}, {net_entity.size_x, net_entity.size_y}});
        entity.addComponent(HealthComponent{net_entity.health, net_entity.maxHealth, false});
        
        if(!net_entity.inventory.empty())
        {
            InventoryComponent inventory_component(net_entity.inventory.size());

            for(size_t i = 0; i < net_entity.inventory.size(); i++)
            {
                inventory_component.inventory.slots[i].itemID = static_cast<ItemID>(net_entity.inventory[i].itemID);
                inventory_component.inventory.slots[i].quantity = net_entity.inventory[i].quantity;
            }
            inventory_component.selectedSlot = net_entity.selectedSlot;

            entity.addComponent(std::move(inventory_component));
        }

        entities.push_back(std::move(entity));
    }

    playerUIInitialized = false;
    tryInitializePlayerUI();
}

void ClientGameState::processIncoming()
{
    if (!transport) return;

    std::vector<ReceivedPacket> packets;
    try { packets = transport->poll(); }
    catch (const std::bad_alloc&) { errorMessage = "Out of memory"; connectionFailed = true; return; }

    if (isLocalSession())
    {
        return;
    }

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

                    auto& chunk = world.getChunks()[init.chunk.chunk_position];
                    chunk = init.chunk;

                    chunk.meshDirty = true;
                    world.chunkMeshes[init.chunk.chunk_position].built = false;
                    initialized = true;

                    break;
                }
                case PacketType::Snapshot:
                {
                    auto snapshot = deserializeSnapshot(reader);
                    rebuildEntitiesFromSnapshot(snapshot);

                    break;
                }
                case PacketType::BlockUpdate:
                {
                    auto block_update = deserializeBlockUpdate(reader);
                    world.setBlock(block_update.x, block_update.y, block_update.block);
                    break;
                }
                case PacketType::Spawn:
                {
                    auto spawn = deserializeSpawn(reader);
                    myEntityId = spawn.id;
                    localPlayerEntityId = myEntityId;
                    break;
                }
                case PacketType::Despawn:
                {
                    auto despawn = deserializeDespawn(reader);
                    auto& entities = world.getEntities();
                    entities.erase(std::remove_if(entities.begin(), entities.end(), [&](const Entity& e) { return e.getID() == despawn.id; }), entities.end());
                    
                    break;
                }
                case PacketType::Input:
                    
                    break;
            }
        }
        catch (const std::bad_alloc&)
        {
            std::cerr << "[Client] bad_alloc decoding packet\n";
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

void ClientGameState::sendTickInputs()
{
    if (!transport || !transport->isConnected()) return;
    if (myEntityId == 0) return;

    auto polled = getInputs(world, game->getWindow());
    inputs.insert(inputs.end(), std::make_move_iterator(polled.begin()), std::make_move_iterator(polled.end()));

    if (inputs.empty()) return;

    InputPacket packet;
    packet.id = myEntityId;
    packet.inputs = std::move(inputs);
    inputs.clear();

    transport->send(serializePacket(packet));
}

void ClientGameState::onTick(float tick_step)
{
    if (transport && transport->isConnected())
    {
        sendTickInputs();
    }
    else if (localServer)
    {
        auto polled = ::getInputs(world, game->getWindow());
        inputs.insert(inputs.end(),
            std::make_move_iterator(polled.begin()),
            std::make_move_iterator(polled.end()));

        if (!inputs.empty() && localPlayerEntityId.has_value())
        {
            processWorldInputs(world, std::move(inputs), localPlayerEntityId.value());
            inputs.clear();
        }
    }

    if (localServer)
    {
        localServer->tick(tick_step);
    }

    if (transport)
    {
        processIncoming();
    }
}

void ClientGameState::update(float dt)
{
    if (!isLocalSession())
    {
        try
        {
            if (!connectAttempted && !connectionFailed)
            {
                connectAttempted = true;
                if (!transport->connect(pendingHost, pendingPort))
                {
                    connectionFailed = true;
                    errorMessage = transport->getLastError();
                    if (errorMessage.empty()) errorMessage = "Connection failed";
                    std::cerr << "[Client] connect failed: " << errorMessage << '\n';
                    return;
                }
                wasConnected = true;
                std::cerr << "[Client] connected to " << remoteAddress << '\n';
            }

            if (wasConnected && !loginSent && transport->isConnected())
            {
                transport->send(serializePacket(LoginPacket{nickname}));
                loginSent = true;
                std::cerr << "[Client] Sent Login as \"" << nickname << "\"\n";
            }

            if (!connectionFailed && wasConnected && !transport->isConnected())
            {
                connectionFailed = true;
                if (errorMessage.empty())
                {
                    errorMessage = transport->getLastError();
                    if (errorMessage.empty()) errorMessage = "Disconnected from server";
                }
            }

            if (connectionFailed) return;
        }
        catch (const std::bad_alloc&)
        {
            std::cerr << "[Client] bad_alloc in update\n";
            connectionFailed = true;
            errorMessage = "Out of memory while updating";
            return;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Client] exception in update: " << e.what() << '\n';
            connectionFailed = true;
            errorMessage = e.what();
            return;
        }
    }

    MainGameState::update(dt);
}

void ClientGameState::handleEvent(const sf::Event& event)
{
    if (auto key = event.getIf<sf::Event::KeyPressed>())
    {
        if (!isLocalSession() && key->code == sf::Keyboard::Key::Escape && (connectionFailed || (transport && !transport->isConnected())))
        {
            game->popState();
            return;
        }

        if (key->code == sf::Keyboard::Key::F1)
        {
            hideUI = !hideUI;
        }
        else if (key->code == sf::Keyboard::Key::F2)
        {
            pendingScreenshot = true;
        }
    }

    MainGameState::handleEvent(event);

    uint8_t* slotPtr = nullptr;
    if (hasPlayerEntity())
    {
        auto& playerEntity = entityWithID(localPlayerEntityId.value(), world);
        if (playerEntity.hasComponent<InventoryComponent>())
        {
            slotPtr = &playerEntity.getComponent<InventoryComponent>().selectedSlot;
        }
    }
    if (!slotPtr) slotPtr = &localSelectedSlot;

    auto new_inputs = ::getInputsFromEvent(event, world, game->getWindow(), *slotPtr);
    inputs.insert(inputs.end(),
        std::make_move_iterator(new_inputs.begin()),
        std::make_move_iterator(new_inputs.end()));
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

    if (!isLocalSession() && !initialized)
    {
        auto [skyTop, skyBottom] = world.getSkyGradient(world.getDayTime() / World::DAY_CYCLE_DURATION);
        renderSky(window, skyTop, skyBottom);

        sf::Text waiting(AssetManager::getFont(0), "Connecting to " + remoteAddress + "...", 28);
        waiting.setFillColor(sf::Color::White);
        waiting.setOutlineColor(sf::Color::Black);
        waiting.setOutlineThickness(2.0f);
        auto wb = waiting.getLocalBounds();
        waiting.setPosition({(window.getSize().x - wb.size.x) * 0.5f, window.getSize().y * 0.5f});
        window.draw(waiting);
        return;
    }

    MainGameState::render(window);

    if (pendingScreenshot)
    {
        pendingScreenshot = false;
        saveScreenshot(window);
    }
}

void ClientGameState::saveScreenshot(sf::RenderWindow& window)
{
    std::filesystem::path dir;

    #ifdef _WIN32
        const char* appdata = std::getenv("APPDATA");
        dir = appdata ? std::filesystem::path(appdata) : std::filesystem::temp_directory_path();
    #else
        const char* home = std::getenv("HOME");
        dir = home ? std::filesystem::path(home) : std::filesystem::temp_directory_path();
    #endif

    dir /= "Blockbit";
    dir /= "screenshots";

    try
    {
        std::filesystem::create_directories(dir);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Client] Failed to create screenshot directory: " << e.what() << '\n';
        return;
    }

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_local{};
    #ifdef _WIN32
        localtime_s(&tm_local, &t);
    #else
        localtime_r(&t, &tm_local);
    #endif

    std::ostringstream name;
    name << "screenshot_" << std::put_time(&tm_local, "%Y-%m-%d_%H-%M-%S") << ".png";

    sf::Vector2u size = window.getSize();
    if (size.x == 0 || size.y == 0) return;

    sf::Texture texture(size);
    texture.update(window);

    std::filesystem::path filePath = dir / name.str();
    if (!texture.copyToImage().saveToFile(filePath.string()))
    {
        std::cerr << "[Client] Failed to save screenshot to " << filePath << '\n';
        return;
    }

    std::cerr << "[Client] Screenshot saved: " << filePath << '\n';
}
