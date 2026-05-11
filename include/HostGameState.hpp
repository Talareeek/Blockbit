#ifndef HOST_GAME_STATE_HPP
#define HOST_GAME_STATE_HPP

#include "MainGameState.hpp"
#include "Server.hpp"

#include <unordered_map>
#include <unordered_set>
#include <cstdint>

class HostGameState : public MainGameState
{
private:

    Server server;

    std::unordered_map<uint32_t, uint32_t> clientToEntity;
    std::unordered_set<uint32_t> knownClients;

    struct RemoteInput
    {
        bool left = false;
        bool right = false;
        bool jumpHeld = false;
        bool jumpPressed = false;
    };
    std::unordered_map<uint32_t, RemoteInput> remoteInputs;

    float snapshotTimer = 0.0f;

    void spawnRemotePlayer(uint32_t clientId);
    void despawnRemotePlayer(uint32_t clientId);

    void sendInitializationTo(uint32_t clientId, int aroundChunkPos);

    void syncConnections();
    void processIncoming();
    void applyRemoteInputs(float dt);
    void broadcastBlockUpdates();
    void broadcastSnapshot();

public:

    HostGameState(Game* game, World world, uint16_t port = DEFAULT_PORT);
    ~HostGameState() override;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;

    static constexpr float    SNAPSHOT_INTERVAL = 0.05f;
    static constexpr uint16_t DEFAULT_PORT      = 25565;
};

#endif // HOST_GAME_STATE_HPP
