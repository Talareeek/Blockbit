#include "../include/NetworkServerTransport.hpp"

NetworkServerTransport::NetworkServerTransport(uint16_t port)
    : server(port)
{
}

NetworkServerTransport::~NetworkServerTransport() = default;

void NetworkServerTransport::start()
{
    server.start();
}

void NetworkServerTransport::stop()
{
    server.stop();
}

std::vector<IncomingPacket> NetworkServerTransport::poll()
{
    return server.poll();
}

void NetworkServerTransport::send(uint32_t clientId, std::vector<char> buf)
{
    server.send(clientId, std::move(buf));
}

void NetworkServerTransport::broadcast(const std::vector<char>& buf)
{
    server.broadcast(buf);
}

void NetworkServerTransport::broadcastExcept(uint32_t clientId, const std::vector<char>& buf)
{
    server.broadcastExcept(clientId, buf);
}

std::vector<uint32_t> NetworkServerTransport::clientIds()
{
    return server.clientIds();
}

bool NetworkServerTransport::isRunning() const
{
    return server.isRunning();
}
