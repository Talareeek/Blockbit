#ifndef SERVER_TRANSPORT_HPP
#define SERVER_TRANSPORT_HPP

#include "Server.hpp" // for IncomingPacket

#include <vector>
#include <cstdint>

class ServerTransport
{
public:

    virtual ~ServerTransport() = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    virtual std::vector<IncomingPacket> poll() = 0;

    virtual void send(uint32_t clientId, std::vector<char> buf) = 0;
    virtual void broadcast(const std::vector<char>& buf) = 0;
    virtual void broadcastExcept(uint32_t clientId, const std::vector<char>& buf) = 0;

    virtual std::vector<uint32_t> clientIds() = 0;

    virtual bool isRunning() const = 0;
};

#endif // SERVER_TRANSPORT_HPP
