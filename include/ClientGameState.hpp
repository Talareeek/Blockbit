#ifndef CLIENT_GAME_STATE_HPP
#define CLIENT_GAME_STATE_HPP

#include "GameState.hpp"
#include "World.hpp"
#include "ClientTransport.hpp"
#include "GameServer.hpp"
#include "Packet.hpp"
#include "HealthBar.hpp"
#include "InventoryWidget.hpp"
#include "Hotbar.hpp"
#include "Input.hpp"
#include "Chat.hpp"
#include "ChatUI.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>

class ClientGameState : public GameState
{
private:

    World local_world;

    std::unique_ptr<ClientTransport> transport;
    std::optional<GameServer> local_server;

    std::optional<UUID> local_player_entity_id;
    UUID my_entity_id;

    bool initialized = false;
    bool connection_failed = false;
    bool was_connected = false;
    bool connect_attempted = false;

    std::string error_message;
    std::string remote_address;
    std::string pending_host;
    uint16_t pending_port = 0;
    std::string nickname;
    bool login_sent = false;

    uint8_t local_selected_slot = 0;

    HealthBar health_bar;
    InventoryWidget inventory_widget{nullptr};
    Hotbar hotbar;

    bool player_ui_initialized = false;

    bool debug = false;
    bool hide_ui = false;

    int fps = 0;
    float last_fps_update = 1.0f;

    float since_last_tick = 0.0f;

    std::vector<Input> inputs;

    bool pending_screenshot = false;

    Chat chat;
    ChatUI chat_ui;

    float chat_close_cooldown = 0.0f;

    sf::Vector2<double> camera;

    void tryInitializePlayerUI();
    bool hasPlayerEntity() const;

    void processIncoming();
    void sendTickInputs();
    void rebuildEntitiesFromSnapshot(const SnapshotPacket& snapshot);
    void saveScreenshot(sf::RenderWindow& window);

    void onTick(float tick_step);

    bool acceptsPlayerInput() const { return !chat_ui.isActive() && chat_close_cooldown <= 0.0f; }

    bool isLocalSession() const { return local_server.has_value(); }

    std::string debugString();

public:

    ClientGameState(Game* game, std::filesystem::path world_path, uint16_t network_port, std::string nickname);

    ClientGameState(Game* game, std::string name, unsigned int seed, GenerationProperties generation_properties, uint16_t network_port, std::string nickname);

    ClientGameState(Game* game, const std::string& host, uint16_t port, std::string nickname);

    ~ClientGameState() override;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    bool alwaysUpdated() const override;

    static constexpr uint16_t DEFAULT_PORT = GameServer::DEFAULT_PORT;
};

#endif // CLIENT_GAME_STATE_HPP
