#include "../include/Client.hpp"
#include "../include/Network.hpp"

#include <iostream>

Client::Client() : socket(io)
{
    
}

Client::~Client()
{
    disconnect();
}

bool Client::connect(const std::string& host, uint16_t port, std::chrono::milliseconds timeout)
{
    if (connected) return true;

    last_error.clear();

    try
    {
        asio::ip::tcp::resolver resolver(io);
        asio::ip::tcp::resolver::results_type endpoints;

        try
        {
            endpoints = resolver.resolve(host, std::to_string(port));
        }
        catch (const asio::system_error& e)
        {
            last_error = "DNS: " + std::string(e.code().message());
            std::cerr << "Client resolve failed: " << last_error << '\n';
            io.restart();
            return false;
        }

        struct ConnectState
        {
            std::atomic<bool> done{false};
            asio::error_code err = asio::error::would_block;
        };
        auto state = std::make_shared<ConnectState>();

        asio::async_connect(socket, endpoints,
            [state](const asio::error_code& ec, const asio::ip::tcp::endpoint&)
            {
                state->err = ec;
                state->done = true;
            });

        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!state->done && std::chrono::steady_clock::now() < deadline)
        {
            io.run_for(std::chrono::milliseconds(50));
        }

        if (!state->done)
        {
            asio::error_code ec;
            socket.close(ec);
            io.run();
            io.restart();
            last_error = "Connection timed out";
            std::cerr << "Client connect failed: " << last_error << '\n';
            return false;
        }

        if (state->err)
        {
            asio::error_code ec;
            socket.close(ec);
            io.restart();
            last_error = state->err.message();
            std::cerr << "Client connect failed: " << last_error << '\n';
            return false;
        }

        io.restart();

        connected = true;
        readHeader();

        ioThread = std::thread([this]()
        {
            try
            {
                io.run();
            }
            catch (const std::bad_alloc& e)
            {
                std::cerr << "Client io thread bad_alloc: " << e.what() << '\n';
                last_error = "Out of memory in io thread";
            }
            catch (const std::exception& e)
            {
                std::cerr << "Client io error: " << e.what() << '\n';
                last_error = e.what();
            }
            catch (...)
            {
                std::cerr << "Client io thread: unknown exception\n";
                last_error = "Unknown io thread exception";
            }
            connected = false;
        });

        return true;
    }
    catch (const asio::system_error& e)
    {
        last_error = e.code().message();
        std::cerr << "Client connect failed: " << last_error << '\n';
        asio::error_code ec;
        socket.close(ec);
        io.restart();
        return false;
    }
    catch (const std::exception& e)
    {
        last_error = e.what();
        std::cerr << "Client connect failed: " << last_error << '\n';
        asio::error_code ec;
        socket.close(ec);
        io.restart();
        return false;
    }
}

void Client::disconnect()
{
    bool wasConnected = connected.exchange(false);

    if (wasConnected)
    {
        asio::post(io, [this]()
        {
            asio::error_code ec;
            socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            socket.close(ec);
        });
    }
    else
    {
        asio::error_code ec;
        socket.close(ec);
    }

    if (ioThread.joinable()) ioThread.join();
    io.restart();
}

void Client::handleDisconnect()
{
    connected = false;
    asio::error_code ec;
    socket.close(ec);
}

void Client::readHeader()
{
    asio::async_read(socket, asio::buffer(headerBuf),
        [this](const asio::error_code& ec, std::size_t /*n*/)
        {
            if (ec) { handleDisconnect(); return; }

            uint32_t netSize;
            std::memcpy(&netSize, headerBuf.data(), sizeof(netSize));
            std::size_t length = ntohl(netSize);

            if (length == 0 || length > MAX_PACKET_SIZE)
            {
                handleDisconnect();
                return;
            }

            readBody(length);
        });
}

void Client::readBody(std::size_t length)
{
    bodyBuf.resize(length);
    asio::async_read(socket, asio::buffer(bodyBuf),
        [this](const asio::error_code& ec, std::size_t /*n*/)
        {
            if (ec) { handleDisconnect(); return; }

            if (!bodyBuf.empty())
            {
                PacketType type = static_cast<PacketType>(static_cast<uint8_t>(bodyBuf[0]));
                std::vector<char> payload(bodyBuf.begin() + 1, bodyBuf.end());
                {
                    std::lock_guard<std::mutex> lock(inboxMutex);
                    inbox.push_back({type, std::move(payload)});
                }
            }

            readHeader();
        });
}

void Client::send(std::vector<char> buf)
{
    if (!connected) return;

    auto framed = encode_buffer(buf);
    bool startWrite = false;
    {
        std::lock_guard<std::mutex> lock(writeMutex);
        writeQueue.push_back(std::move(framed));
        if (!writing)
        {
            writing = true;
            startWrite = true;
        }
    }
    if (startWrite)
    {
        asio::post(io, [this]() { doWrite(); });
    }
}

void Client::doWrite()
{
    std::vector<char>* current = nullptr;
    {
        std::lock_guard<std::mutex> lock(writeMutex);
        if (writeQueue.empty())
        {
            writing = false;
            return;
        }
        current = &writeQueue.front();
    }

    asio::async_write(socket, asio::buffer(*current),
        [this](const asio::error_code& ec, std::size_t /*n*/)
        {
            if (ec) { handleDisconnect(); return; }
            {
                std::lock_guard<std::mutex> lock(writeMutex);
                writeQueue.pop_front();
            }
            doWrite();
        });
}

std::vector<ReceivedPacket> Client::poll()
{
    std::vector<ReceivedPacket> out;
    std::lock_guard<std::mutex> lock(inboxMutex);
    out.assign(std::make_move_iterator(inbox.begin()),
               std::make_move_iterator(inbox.end()));
    inbox.clear();
    return out;
}
