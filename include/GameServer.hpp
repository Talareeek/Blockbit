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
#include <functional>
#include <cstdint>

class GameServer
{
private:

    World world;
    
    std::unique_ptr<ServerTransport> transport;
    uint32_t host_client_id;
    uint32_t host_entity_id{0};
    bool save_on_destruct = true;

    std::unordered_map<uint32_t, uint32_t> client_to_entity;
    std::unordered_map<uint32_t, std::string> client_to_nickname;

    std::unordered_set<uint32_t> known_clients;
    std::unordered_map<uint32_t, std::deque<std::vector<Input>>> remote_input_queues;
    std::unordered_map<uint32_t, std::unordered_set<int>> sent_chunks;

    void spawnRemotePlayer(uint32_t client_id, const std::string& nickname);
    void despawnRemotePlayer(uint32_t client_id);
    void sendInitializationTo(uint32_t client_id, int around_chunk_position);
    void streamChunksToClients();

    void syncConnections();
    void processIncoming();
    void runSystems(float tick_step);
    void broadcastBlockUpdates();
    void broadcastSnapshot();

public:

    GameServer(World world, std::unique_ptr<ServerTransport> transport, uint32_t host_client_id = 0);
    ~GameServer();

    void tick(float tick_step);

    World& getWorld() { return world; }

    uint32_t getHostEntityId() const { return host_entity_id; }

    void sendChat(const std::wstring& nickname, const std::wstring& message);

    std::function<void(std::wstring)> onChatBroadcast;

    static constexpr uint16_t DEFAULT_PORT = 25565;
};

extern bool isNicknameAllowed(std::string nickname);

#endif // GAME_SERVER_HPP
