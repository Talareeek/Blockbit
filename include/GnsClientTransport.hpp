#ifndef GNS_CLIENT_TRANSPORT_HPP
#define GNS_CLIENT_TRANSPORT_HPP

#include "ClientTransport.hpp"

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
    bool connected = false;

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
    const std::string& getLastError() const override;
};

#endif // GNS_CLIENT_TRANSPORT_HPP
