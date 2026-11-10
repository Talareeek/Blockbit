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

    // Default impl matches the synchronous transports (TCP, loopback): once
    // connect() returns, we are either connected or failed, never "in progress".
    // Async transports (GNS) override this to return true while the handshake
    // is mid-flight so callers don't trip a false negative on isConnected().
    virtual bool isConnecting() const { return false; }

    virtual const std::string& getLastError() const = 0;
};

#endif // CLIENT_TRANSPORT_HPP
