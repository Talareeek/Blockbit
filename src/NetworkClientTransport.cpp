#include "../include/NetworkClientTransport.hpp"

NetworkClientTransport::NetworkClientTransport(std::chrono::milliseconds connectTimeout)
    : connectTimeout(connectTimeout)
{
}

NetworkClientTransport::~NetworkClientTransport() = default;

bool NetworkClientTransport::connect(const std::string& host, uint16_t port)
{
    return client.connect(host, port, connectTimeout);
}

void NetworkClientTransport::disconnect()
{
    client.disconnect();
}

void NetworkClientTransport::send(std::vector<char> buf)
{
    client.send(std::move(buf));
}

std::vector<ReceivedPacket> NetworkClientTransport::poll()
{
    return client.poll();
}

bool NetworkClientTransport::isConnected() const
{
    return client.isConnected();
}

const std::string& NetworkClientTransport::getLastError() const
{
    return client.getLastError();
}
