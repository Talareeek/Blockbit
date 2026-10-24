#ifndef SERVER_TRANSPORT_HPP
#define SERVER_TRANSPORT_HPP

#include "Server.hpp" // for IncomingPacket

#include <vector>
#include <string>
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

    virtual void disconnect(uint32_t clientId) = 0;

    // Reason-carrying variant. Transports that can deliver a disconnect reason
    // on the wire should override; the default falls through to the plain
    // disconnect since GameServer already pushes a ChatMessagePacket with the
    // reason right before calling this.
    virtual void disconnect(uint32_t clientId, const std::wstring& /*reason*/)
    {
        disconnect(clientId);
    }

    virtual std::vector<uint32_t> clientIds() = 0;

    virtual bool isRunning() const = 0;
};

#endif // SERVER_TRANSPORT_HPP
