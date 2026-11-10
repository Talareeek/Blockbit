#ifndef GNS_CLIENT_TRANSPORT_HPP
#define GNS_CLIENT_TRANSPORT_HPP

#include "ClientTransport.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

class GnsClientTransport : public ClientTransport
{
private:

    HSteamNetConnection connection = k_HSteamNetConnection_Invalid;

    std::deque<ReceivedPacket> inbox;
    std::mutex state_mutex;

    std::string last_error;
    bool connected  = false;
    bool connecting = false;

    // Self-timeout for the async handshake. isConnecting() flips to false after
    // this elapses without Connected; caller then sees !isConnected() and goes
    // to the failed path, matching the old blocking-connect UX.
    std::chrono::steady_clock::time_point connect_deadline{};

    void onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);
    static void staticStatusCallback(SteamNetConnectionStatusChangedCallback_t* info);

public:

    GnsClientTransport();
    ~GnsClientTransport() override;

    bool connect(const std::string& host, uint16_t port) override;
    void disconnect() override;

    void send(std::vector<char> buf) override;
    std::vector<ReceivedPacket> poll() override;

    bool isConnected() const override;
    bool isConnecting() const override;
    const std::string& getLastError() const override;
};

#endif // GNS_CLIENT_TRANSPORT_HPP
