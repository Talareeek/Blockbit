#ifndef SERVER_HPP
#define SERVER_HPP

#include <asio.hpp>

#include <memory>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <deque>
#include <unordered_map>
#include <array>
#include <cstdint>

#include "Packet.hpp"

class Server;

struct IncomingPacket
{
    uint32_t     clientId;
    PacketType   type;
    std::vector<char> payload;
};

class ServerSession : public std::enable_shared_from_this<ServerSession>
{
private:

    asio::ip::tcp::socket socket;
    Server& server;
    uint32_t clientId;

    std::array<char, 4> headerBuf;
    std::vector<char>   bodyBuf;

    std::deque<std::vector<char>> writeQueue;
    std::mutex writeMutex;
    bool writing = false;

    void readHeader();
    void readBody(std::size_t length);
    void doWrite();

public:

    ServerSession(asio::ip::tcp::socket s, Server& server, uint32_t id);

    void start();
    void send(std::vector<char> buf);
    void close();

    uint32_t getId() const { return clientId; }
};

class Server
{
private:

    asio::io_context io;
    asio::ip::tcp::acceptor acceptor;
    std::thread ioThread;
    std::atomic<bool> running{false};

    std::unordered_map<uint32_t, std::shared_ptr<ServerSession>> sessions;
    std::mutex sessionsMutex;

    std::deque<IncomingPacket> inbox;
    std::mutex inboxMutex;

    uint32_t nextClientId = 100;

    void doAccept();

public:

    explicit Server(uint16_t port);
    ~Server();

    void start();
    void stop();

    std::vector<IncomingPacket> poll();

    void send(uint32_t clientId, std::vector<char> buf);
    void broadcast(const std::vector<char>& buf);
    void broadcastExcept(uint32_t clientId, const std::vector<char>& buf);

    void removeSession(uint32_t clientId);
    void deliver(IncomingPacket pkt);

    std::vector<uint32_t> clientIds();

    bool isRunning() const { return running; }
};

#endif // SERVER_HPP
