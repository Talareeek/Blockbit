#ifndef LOOPBACK_SERVER_TRANSPORT_HPP
#define LOOPBACK_SERVER_TRANSPORT_HPP

#include "ServerTransport.hpp"
#include "ClientTransport.hpp"

#include <deque>
#include <mutex>
#include <memory>
#include <vector>
#include <cstdint>

class LoopbackClientTransport;

class LoopbackChannel
{
public:

    static constexpr uint32_t LOOPBACK_CLIENT_ID = 1;

    std::mutex mutex;
    std::deque<IncomingPacket> toServer;
    std::deque<ReceivedPacket>  toClient;

    bool clientConnected = false;
};

class LoopbackServerTransport : public ServerTransport
{
private:

    std::shared_ptr<LoopbackChannel> channel;
    bool running = false;

public:

    explicit LoopbackServerTransport(std::shared_ptr<LoopbackChannel> channel);
    ~LoopbackServerTransport() override;

    void start() override;
    void stop() override;

    std::vector<IncomingPacket> poll() override;

    void send(uint32_t clientId, std::vector<char> buf) override;
    void broadcast(const std::vector<char>& buf) override;
    void broadcastExcept(uint32_t clientId, const std::vector<char>& buf) override;

    std::vector<uint32_t> clientIds() override;

    bool isRunning() const override;
};

class LoopbackClientTransport : public ClientTransport
{
private:

    std::shared_ptr<LoopbackChannel> channel;
    bool connected = false;
    std::string last_error;

public:

    explicit LoopbackClientTransport(std::shared_ptr<LoopbackChannel> channel);
    ~LoopbackClientTransport() override;

    bool connect(const std::string& host, uint16_t port) override;
    void disconnect() override;

    void send(std::vector<char> buf) override;
    std::vector<ReceivedPacket> poll() override;

    bool isConnected() const override;
    const std::string& getLastError() const override;
};

struct LoopbackPair
{
    std::unique_ptr<LoopbackServerTransport> serverSide;
    std::unique_ptr<LoopbackClientTransport> clientSide;
};

LoopbackPair makeLoopbackPair();

#endif // LOOPBACK_SERVER_TRANSPORT_HPP
