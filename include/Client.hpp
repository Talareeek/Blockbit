#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <asio.hpp>

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <chrono>

#include "Packet.hpp"

struct ReceivedPacket
{
    PacketType type;
    std::vector<char> payload;
};

class Client
{
private:

    asio::io_context io;
    asio::ip::tcp::socket socket;
    std::thread ioThread;
    std::atomic<bool> connected{false};

    std::array<char, 4> headerBuf;
    std::vector<char> bodyBuf;

    std::deque<std::vector<char>> writeQueue;
    std::mutex writeMutex;
    bool writing = false;

    std::deque<ReceivedPacket> inbox;
    std::mutex inboxMutex;

    void readHeader();
    void readBody(std::size_t length);
    void doWrite();
    void handleDisconnect();

public:

    Client();
    ~Client();

    bool connect(const std::string& host, uint16_t port, std::chrono::milliseconds timeout = std::chrono::seconds(5));
    void disconnect();

    void send(std::vector<char> buf);
    std::vector<ReceivedPacket> poll();

    bool isConnected() const { return connected; }
    const std::string& getLastError() const { return last_error; }

private:

    std::string last_error;
};

#endif // CLIENT_HPP
