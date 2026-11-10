#ifndef GNS_SERVER_TRANSPORT_HPP
#define GNS_SERVER_TRANSPORT_HPP

#include "ServerTransport.hpp"

#include <cstdint>
#include <deque>
#include <mutex>
#include <unordered_map>

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

// Server-side transport using the open-source GameNetworkingSockets library.
// Matches the ISteamNetworkingSockets API used by Steam's SDR transport, so
// the Steam SDK path can be swapped in later by changing init + the factory
// returning the Steam-flavoured ISteamNetworkingSockets singleton.
class GnsServerTransport : public ServerTransport
{
private:

    uint16_t port;

    HSteamListenSocket listen_sock = k_HSteamListenSocket_Invalid;
    HSteamNetPollGroup poll_group  = k_HSteamNetPollGroup_Invalid;

    uint32_t next_client_id = 100;

    // Two-way map so broadcastExcept / disconnect-by-id stay O(1) and the
    // status callback can translate HSteamNetConnection -> clientId in O(1).
    std::unordered_map<uint32_t, HSteamNetConnection> id_to_conn;
    std::unordered_map<HSteamNetConnection, uint32_t> conn_to_id;

    std::deque<IncomingPacket> inbox;
    std::mutex state_mutex;

    bool running = false;

    void onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);
    static void staticStatusCallback(SteamNetConnectionStatusChangedCallback_t* info);

public:

    explicit GnsServerTransport(uint16_t port);
    ~GnsServerTransport() override;

    void start() override;
    void stop() override;

    std::vector<IncomingPacket> poll() override;

    void send(uint32_t clientId, std::vector<char> buf) override;
    void broadcast(const std::vector<char>& buf) override;
    void broadcastExcept(uint32_t clientId, const std::vector<char>& buf) override;

    void disconnect(uint32_t clientId) override;

    std::vector<uint32_t> clientIds() override;

    bool isRunning() const override;
};

#endif // GNS_SERVER_TRANSPORT_HPP
