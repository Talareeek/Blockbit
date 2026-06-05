#ifndef CLIENT_GAME_STATE_HPP
#define CLIENT_GAME_STATE_HPP

#include "GameState.hpp"
#include "Client.hpp"
#include "World.hpp"
#include "Input.hpp"

#include <string>
#include <vector>
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

    std::vector<Input> inputs;
    float since_last_tick = 0.0f;
    uint8_t localSelectedSlot = 0;

    void processIncoming();
    void sendTickInputs();
    void rebuildEntitiesFromSnapshot(const SnapshotPacket& snap);

public:

    ClientGameState(Game* game, const std::string& host, uint16_t port);
    ~ClientGameState() override;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    static constexpr unsigned int UNIT_SIZE_FACTOR = 12;
    static constexpr uint8_t      TICKS_PER_SECOND = 60;
};

#endif // CLIENT_GAME_STATE_HPP
