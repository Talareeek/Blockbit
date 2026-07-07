#ifndef COMPOSITE_SERVER_TRANSPORT_HPP
#define COMPOSITE_SERVER_TRANSPORT_HPP

#include "ServerTransport.hpp"

#include <memory>
#include <vector>

class CompositeServerTransport : public ServerTransport
{
private:

    std::vector<std::unique_ptr<ServerTransport>> transports;

public:

    void add(std::unique_ptr<ServerTransport> transport);

    void start() override;
    void stop() override;

    std::vector<IncomingPacket> poll() override;

    void send(uint32_t client_id, std::vector<char> buffer) override;
    void broadcast(const std::vector<char>& buffer) override;
    void broadcastExcept(uint32_t client_id, const std::vector<char>& buffer) override;

    std::vector<uint32_t> clientIds() override;

    bool isRunning() const override;
};

#endif // COMPOSITE_SERVER_TRANSPORT_HPP
