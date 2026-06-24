#ifndef NETWORK_CLIENT_TRANSPORT_HPP
#define NETWORK_CLIENT_TRANSPORT_HPP

#include "ClientTransport.hpp"
#include "Client.hpp"

#include <chrono>

class NetworkClientTransport : public ClientTransport
{
private:

    Client client;
    std::chrono::milliseconds connectTimeout;

public:

    explicit NetworkClientTransport(std::chrono::milliseconds connectTimeout = std::chrono::seconds(3));
    ~NetworkClientTransport() override;

    bool connect(const std::string& host, uint16_t port) override;
    void disconnect() override;

    void send(std::vector<char> buf) override;
    std::vector<ReceivedPacket> poll() override;

    bool isConnected() const override;
    const std::string& getLastError() const override;
};

#endif // NETWORK_CLIENT_TRANSPORT_HPP
