#include "../include/CompositeServerTransport.hpp"

#include <iterator>

void CompositeServerTransport::add(std::unique_ptr<ServerTransport> transport)
{
    transports.push_back(std::move(transport));
}

void CompositeServerTransport::start()
{
    for (auto& transport : transports) transport->start();
}

void CompositeServerTransport::stop()
{
    for (auto& transport : transports) transport->stop();
}

std::vector<IncomingPacket> CompositeServerTransport::poll()
{
    std::vector<IncomingPacket> combined;
    for (auto& transport : transports)
    {
        auto packets = transport->poll();
        combined.insert(combined.end(),
            std::make_move_iterator(packets.begin()),
            std::make_move_iterator(packets.end()));
    }
    return combined;
}

void CompositeServerTransport::send(uint32_t client_id, std::vector<char> buffer)
{
    for (auto& transport : transports)
    {
        std::vector<char> copy = buffer;
        transport->send(client_id, std::move(copy));
    }
}

void CompositeServerTransport::broadcast(const std::vector<char>& buffer)
{
    for (auto& transport : transports) transport->broadcast(buffer);
}

void CompositeServerTransport::broadcastExcept(uint32_t client_id, const std::vector<char>& buffer)
{
    for (auto& transport : transports) transport->broadcastExcept(client_id, buffer);
}

std::vector<uint32_t> CompositeServerTransport::clientIds()
{
    std::vector<uint32_t> combined;
    for (auto& transport : transports)
    {
        auto ids = transport->clientIds();
        combined.insert(combined.end(), ids.begin(), ids.end());
    }
    return combined;
}

bool CompositeServerTransport::isRunning() const
{
    for (auto& transport : transports)
    {
        if (!transport->isRunning()) return false;
    }
    return true;
}
