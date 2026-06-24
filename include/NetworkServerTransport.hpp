#ifndef NETWORK_SERVER_TRANSPORT_HPP
#define NETWORK_SERVER_TRANSPORT_HPP

#include "ServerTransport.hpp"
#include "Server.hpp"

#include <cstdint>

class NetworkServerTransport : public ServerTransport
{
private:

    Server server;

public:

    explicit NetworkServerTransport(uint16_t port);
    ~NetworkServerTransport() override;

    void start() override;
    void stop() override;

    std::vector<IncomingPacket> poll() override;

    void send(uint32_t clientId, std::vector<char> buf) override;
    void broadcast(const std::vector<char>& buf) override;
    void broadcastExcept(uint32_t clientId, const std::vector<char>& buf) override;

    std::vector<uint32_t> clientIds() override;

    bool isRunning() const override;
};

#endif // NETWORK_SERVER_TRANSPORT_HPP
