#ifndef HOST_GAME_STATE_HPP
#define HOST_GAME_STATE_HPP

#include "MainGameState.hpp"
#include "Server.hpp"

#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <vector>
#include <cstdint>

class HostGameState : public MainGameState
{
private:

    Server server;

    std::unordered_map<uint32_t, uint32_t> clientToEntity;
    std::unordered_set<uint32_t> knownClients;

    std::unordered_map<uint32_t, std::deque<std::vector<Input>>> remoteInputQueues;

    void spawnRemotePlayer(uint32_t clientId);
    void despawnRemotePlayer(uint32_t clientId);

    void sendInitializationTo(uint32_t clientId, int aroundChunkPos);

    void syncConnections();
    void processIncoming();
    void broadcastBlockUpdates();
    void broadcastSnapshot();

protected:

    void onTick(float tick_step) override;

public:

    HostGameState(Game* game, World world, uint16_t port = DEFAULT_PORT);
    ~HostGameState() override;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;

    static constexpr uint16_t DEFAULT_PORT = 25565;
};

#endif // HOST_GAME_STATE_HPP
