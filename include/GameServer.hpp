#ifndef GAME_SERVER_HPP
#define GAME_SERVER_HPP

#include "World.hpp"
#include "ServerTransport.hpp"
#include "Input.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <vector>
#include <cstdint>

class GameServer
{
private:

    World& world;
    std::unique_ptr<ServerTransport> transport;
    uint32_t hostClientId;
    uint32_t hostEntityId{0};

    std::unordered_map<uint32_t, uint32_t> clientToEntity;
    std::unordered_set<uint32_t> knownClients;
    std::unordered_map<uint32_t, std::deque<std::vector<Input>>> remoteInputQueues;
    std::unordered_map<uint32_t, std::unordered_set<int>> sentChunks;

    void spawnRemotePlayer(uint32_t clientId, const std::string& nickname);
    void despawnRemotePlayer(uint32_t clientId);
    void sendInitializationTo(uint32_t clientId, int aroundChunkPos);
    void streamChunksToClients();

    void syncConnections();
    void processIncoming();
    void runSystems(float tick_step);
    void broadcastBlockUpdates();
    void broadcastSnapshot();

public:

    GameServer(World& world, std::unique_ptr<ServerTransport> transport, uint32_t hostClientId = 0);
    ~GameServer();

    void tick(float tick_step);

    World& getWorld() { return world; }

    uint32_t getHostEntityId() const { return hostEntityId; }

    static constexpr uint16_t DEFAULT_PORT = 25565;
};

#endif // GAME_SERVER_HPP
