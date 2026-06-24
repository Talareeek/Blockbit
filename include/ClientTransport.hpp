#ifndef CLIENT_TRANSPORT_HPP
#define CLIENT_TRANSPORT_HPP

#include "Client.hpp" // for ReceivedPacket

#include <vector>
#include <string>
#include <cstdint>

class ClientTransport
{
public:

    virtual ~ClientTransport() = default;

    virtual bool connect(const std::string& host, uint16_t port) = 0;
    virtual void disconnect() = 0;

    virtual void send(std::vector<char> buf) = 0;
    virtual std::vector<ReceivedPacket> poll() = 0;

    virtual bool isConnected() const = 0;
    virtual const std::string& getLastError() const = 0;
};

#endif // CLIENT_TRANSPORT_HPP
