#ifndef GAME_SERVER_HPP
#define GAME_SERVER_HPP

#include <filesystem>

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

enum class ServerMode
{
    Listen,
    Dedicated
};

class GameServer
{
private:

    ServerMode mode;

    std::filesystem::path server_path;

    World world;
    
    std::unique_ptr<ServerTransport> transport;
    bool save_on_destruct = true;

    std::unordered_map<uint32_t, std::string> client_to_nickname;
    std::unordered_map<std::string, uint32_t> nickname_to_entity;

    std::unordered_set<uint32_t> known_clients;
    std::unordered_map<uint32_t, std::deque<std::vector<Input>>> remote_input_queues;
    std::unordered_map<uint32_t, std::unordered_set<int>> sent_chunks;

    void spawnPlayerFor(std::string nickname);
    void deactivatePlayerFor(std::string nickname);

    void sendInitializationTo(uint32_t client_id, int around_chunk_position);
    void streamChunksToClients();

    void syncConnections();
    void processIncoming();
    void broadcastBlockUpdates();
    void broadcastSnapshot();

public:

    GameServer(std::filesystem::path world_path, std::unique_ptr<ServerTransport> transport);
    GameServer(std::string name, unsigned int seed, GenerationProperties generation_properties, std::unique_ptr<ServerTransport> transport);

    //GameServer(std::filesystem::path server_path, std::unique_ptr<ServerTransport> transport);

    void update(float dt);

    World& getWorld() { return world; }

    void sendChat(const std::wstring& nickname, const std::wstring& message);

    std::function<void(std::wstring)> onChatBroadcast;

    static constexpr uint16_t DEFAULT_PORT = 34500;

    ~GameServer();
};

extern bool isNicknameAllowed(std::string nickname);

#endif // GAME_SERVER_HPP
