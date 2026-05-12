#ifndef CLIENT_GAME_STATE_HPP
#define CLIENT_GAME_STATE_HPP

#include "GameState.hpp"
#include "Client.hpp"
#include "World.hpp"

#include <string>
#include <cstdint>

class ClientGameState : public GameState
{
private:

    Client client;
    World  world;

    uint32_t myEntityId = 0;
    bool     initialized = false;
    bool     connectionFailed = false;
    bool     wasConnected = false;
    bool     connectAttempted = false;

    std::string errorMessage;
    std::string remoteAddress;
    std::string pendingHost;
    uint16_t    pendingPort = 0;

    InputPacket lastSent{};

    float sendTimer = 0.0f;

    void processIncoming();
    void sendInput();
    void rebuildEntitiesFromSnapshot(const SnapshotPacket& snap);

public:

    ClientGameState(Game* game, const std::string& host, uint16_t port);
    ~ClientGameState() override;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    static constexpr unsigned int UNIT_SIZE_FACTOR = 12;
    static constexpr float        INPUT_INTERVAL   = 0.05f;
};

#endif // CLIENT_GAME_STATE_HPP
