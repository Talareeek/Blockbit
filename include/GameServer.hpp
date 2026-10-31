#ifndef GAME_SERVER_HPP
#define GAME_SERVER_HPP

#include <filesystem>

#include "ServerWorld.hpp"
#include "ServerTransport.hpp"
#include "Input.hpp"
#include "View.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <queue>
#include <vector>
#include <functional>
#include <cstdint>

enum class ServerMode
{
    Listen,
    Dedicated
};

struct ClientData
{
    std::unordered_set<int> known_chunks;
    std::unordered_set<UUID> known_entities;
    std::string nickname;

    std::unique_ptr<ServerView> content_view;
};

struct BanEntry
{
    std::string nickname;
    std::string reason;
    std::string banned_at; // ISO-8601 UTC string, e.g. "2026-10-24T18:32:07Z"
};

class GameServer
{
private:

    ServerMode mode;

    // Where ops.json and banned.json live. For Listen mode this is the world
    // directory (same as today). For Dedicated/headless this is the server
    // directory (worlds/ is a subdirectory of this).
    std::filesystem::path server_path;

    ServerWorld world;

    std::unique_ptr<ServerTransport> transport;
    bool save_on_destruct = true;

    std::unordered_map<uint32_t, ClientData> clients;
    std::unordered_map<std::string, UUID> nickname_to_entity;

    std::unordered_set<uint32_t> known_clients;
    std::unordered_map<uint32_t, std::deque<std::vector<Input>>> remote_input_queues;

    uint16_t tick_rate = 120;
    uint32_t max_ticks_per_frame = 10;
    uint64_t tick = 0;
    sf::Clock update_clock;

    // Block updates run at a fixed 20 Hz regardless of server tick_rate. The
    // pipeline carries its own tick counter so scheduleTick(delay=N) is in
    // 20 Hz units (one block-tick = 50 ms), decoupled from server tick scale.
    static constexpr float BLOCK_UPDATE_STEP = 1.0f / 20.0f;
    float block_update_accumulator = 0.0f;
    uint64_t block_update_tick = 0;

    uint32_t consecutive_tick_exceptions = 0;
    bool unrecoverable = false;

    // Externalized bits of StatusResponse. Headless loads these from
    // config.json; Listen keeps the defaults.
    std::string server_name        = "Blockbit Server";
    std::string server_description = "A server for Blockbit game";
    uint32_t    max_players        = 20;

    void spawnPlayerFor(std::string nickname);
    void respawnPlayerFor(std::string nickname);
    void deactivatePlayerFor(std::string nickname);

    void sendChunkTo(uint32_t client_id, int around_chunk_position);
    void streamChunksToClients();

    void syncConnections();
    void processIncoming();
    void broadcastBlockUpdates();
    void broadcastSnapshot();

    void updatePlayerEntityConnections();

    void dispatchPendingViewOpens();
    void updateContentViews();

    std::unordered_set<std::string> operators;
    std::filesystem::path operatorsFilePath() const;
    std::filesystem::path legacyOperatorsFilePath() const; // ops.txt (pre-migration)
    void loadOperators();
    void saveOperators() const;

    std::unordered_map<std::string, BanEntry> bans;
    std::filesystem::path bansFilePath() const;
    void loadBans();
    void saveBans() const;

public:

    // Listen-mode constructors: server_path doubles as world_path.
    GameServer(std::filesystem::path world_path, std::unique_ptr<ServerTransport> transport);
    GameServer(std::string name, unsigned int seed, GenerationProperties generation_properties, std::unique_ptr<ServerTransport> transport);

    // Dedicated server constructor (legacy): world_path doubles as server_path.
    GameServer(std::filesystem::path world_path, std::unique_ptr<ServerTransport> transport, ServerMode mode);

    // Dedicated server with an explicit server directory (headless with config.json).
    // server_root is where ops.json / banned.json / config.json live;
    // world_path is <server_root>/worlds/<name>/.
    GameServer(std::filesystem::path server_root, std::filesystem::path world_path, std::unique_ptr<ServerTransport> transport);
    GameServer(std::filesystem::path server_root, std::string name, unsigned int seed, GenerationProperties generation_properties, std::unique_ptr<ServerTransport> transport);

    void update(float dt);

    World& getWorld() { return world; }

    void saveWorld() { world.save(); }

    void sendChat(const std::wstring& nickname, const std::wstring& message);

    // Replies to a specific client as a system chat line. Used to deliver
    // command output/errors back to the issuing player.
    void sendSystemMessageTo(uint32_t clientId, const std::wstring& message);

    std::function<void(std::wstring)> onChatBroadcast;

    bool isOperator(const std::string& nickname) const;
    void addOperator(const std::string& nickname);
    void removeOperator(const std::string& nickname);

    bool isBanned(const std::string& nickname) const;
    const BanEntry* findBan(const std::string& nickname) const;
    void addBan(const std::string& nickname, const std::string& reason);
    bool removeBan(const std::string& nickname);

    // Look up an active client's connection id by their login nickname. Returns
    // std::nullopt if nobody with that nickname is currently connected.
    std::optional<uint32_t> findClientIdByNickname(const std::string& nickname) const;

    // Returns the player entity UUID for a currently connected nickname,
    // std::nullopt if the nickname isn't logged in or the entity is gone.
    std::optional<UUID> findEntityByNickname(const std::string& nickname) const;

    // Sends a system message with the reason, then disconnects. Returns true if
    // the client was found and kicked, false otherwise.
    bool kickClient(const std::string& nickname, const std::wstring& reason);

    void applyServerName(std::string name)        { server_name = std::move(name); }
    void applyServerDescription(std::string desc) { server_description = std::move(desc); }
    void applyMaxPlayers(uint32_t n)              { max_players = n; }
    void applyTickRate(uint16_t rate)             { tick_rate = rate; }
    void applyMaxTicksPerFrame(uint32_t n)        { max_ticks_per_frame = n == 0 ? 1 : n; }

    const std::string& getServerName() const        { return server_name; }
    const std::string& getServerDescription() const { return server_description; }
    uint32_t           getMaxPlayers() const        { return max_players; }
    uint32_t           getMaxTicksPerFrame() const  { return max_ticks_per_frame; }

    ServerMode getMode() const { return mode; }

    static constexpr uint16_t DEFAULT_PORT = 34500;
    static constexpr uint32_t MAX_CONSECUTIVE_TICK_EXCEPTIONS = 100;

    ~GameServer();


    uint16_t getTickRate() const;

    float getTickStep() const;

    bool isUnrecoverable() const { return unrecoverable; }
};

extern bool isNicknameAllowed(std::string nickname);

#endif // GAME_SERVER_HPP
