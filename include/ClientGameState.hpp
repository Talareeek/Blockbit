#ifndef CLIENT_GAME_STATE_HPP
#define CLIENT_GAME_STATE_HPP

#include "MainGameState.hpp"
#include "ClientTransport.hpp"
#include "GameServer.hpp"
#include "Packet.hpp"
#include "Chat.hpp"
#include "ChatUI.hpp"

#include <memory>
#include <optional>
#include <string>
#include <cstdint>

class ClientGameState : public MainGameState
{
private:

    std::unique_ptr<ClientTransport> transport;
    
    std::optional<GameServer> localServer;

    uint32_t myEntityId = 0;
    bool initialized = false;
    bool connectionFailed = false;
    bool wasConnected = false;
    bool connectAttempted = false;

    std::string errorMessage;
    std::string remoteAddress;
    std::string pendingHost;
    uint16_t pendingPort = 0;
    std::string nickname;
    bool loginSent = false;

    uint8_t localSelectedSlot = 0;

    bool pendingScreenshot = false;

    Chat chat;
    ChatUI chatUI;

    float chatCloseCooldown = 0.0f;

    void processIncoming();
    void sendTickInputs();
    void rebuildEntitiesFromSnapshot(const SnapshotPacket& snap);
    void saveScreenshot(sf::RenderWindow& window);

    bool isLocalSession() const { return localServer.has_value(); }

protected:

    void onTick(float tick_step) override;

    bool acceptsPlayerInput() const override { return !chatUI.isActive() && chatCloseCooldown <= 0.0f; }

public:

    ClientGameState(Game* game, const std::string& host, uint16_t port, std::string nickname = "Player");

    ClientGameState(Game* game, World world, uint16_t networkPort = 0, std::string nickname = "Player");

    ~ClientGameState() override;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    static constexpr uint16_t DEFAULT_PORT = 25565;
};

#endif // CLIENT_GAME_STATE_HPP
