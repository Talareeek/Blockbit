#include "../include/Server.hpp"
#include "../include/Network.hpp"

#include <iostream>

ServerSession::ServerSession(asio::ip::tcp::socket s, Server& srv, uint32_t id)
    : socket(std::move(s)), server(srv), clientId(id)
{
}

void ServerSession::start()
{
    readHeader();
}

void ServerSession::readHeader()
{
    auto self = shared_from_this();
    asio::async_read(socket, asio::buffer(headerBuf),
        [this, self](const asio::error_code& ec, std::size_t /*n*/)
        {
            if (ec)
            {
                server.removeSession(clientId);
                return;
            }

            uint32_t netSize;
            std::memcpy(&netSize, headerBuf.data(), sizeof(netSize));
            std::size_t length = ntohl(netSize);

            if (length == 0 || length > MAX_PACKET_SIZE)
            {
                server.removeSession(clientId);
                return;
            }

            readBody(length);
        });
}

void ServerSession::readBody(std::size_t length)
{
    bodyBuf.resize(length);
    auto self = shared_from_this();
    asio::async_read(socket, asio::buffer(bodyBuf),
        [this, self](const asio::error_code& ec, std::size_t /*n*/)
        {
            if (ec)
            {
                server.removeSession(clientId);
                return;
            }

            if (!bodyBuf.empty())
            {
                PacketType type = static_cast<PacketType>(static_cast<uint8_t>(bodyBuf[0]));
                std::vector<char> payload(bodyBuf.begin() + 1, bodyBuf.end());
                server.deliver(IncomingPacket{clientId, type, std::move(payload)});
            }

            readHeader();
        });
}

void ServerSession::send(std::vector<char> buf)
{
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
        asio::post(socket.get_executor(),
            [self = shared_from_this()]() { self->doWrite(); });
    }
}

void ServerSession::doWrite()
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

    auto self = shared_from_this();
    asio::async_write(socket, asio::buffer(*current),
        [this, self](const asio::error_code& ec, std::size_t /*n*/)
        {
            if (ec)
            {
                server.removeSession(clientId);
                return;
            }
            {
                std::lock_guard<std::mutex> lock(writeMutex);
                writeQueue.pop_front();
            }
            doWrite();
        });
}

void ServerSession::close()
{
    asio::error_code ec;
    socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    socket.close(ec);
}

// ----- Server -----

Server::Server(uint16_t port)
    : acceptor(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
{
}

Server::~Server()
{
    stop();
}

void Server::start()
{
    if (running.exchange(true)) return;
    doAccept();
    ioThread = std::thread([this]()
    {
        try
        {
            io.run();
        }
        catch (const std::bad_alloc& e)
        {
            std::cerr << "Server io thread bad_alloc: " << e.what() << '\n';
        }
        catch (const std::exception& e)
        {
            std::cerr << "Server io error: " << e.what() << '\n';
        }
        catch (...)
        {
            std::cerr << "Server io thread: unknown exception\n";
        }
    });
}

void Server::stop()
{
    bool wasRunning = running.exchange(false);

    if (wasRunning)
    {
        asio::post(io, [this]()
        {
            asio::error_code ec;
            acceptor.close(ec);

            std::vector<std::shared_ptr<ServerSession>> toClose;
            {
                std::lock_guard<std::mutex> lock(sessionsMutex);
                toClose.reserve(sessions.size());
                for (auto& [id, sess] : sessions) toClose.push_back(sess);
                sessions.clear();
            }
            for (auto& s : toClose) s->close();
        });
    }

    if (ioThread.joinable()) ioThread.join();
    io.restart();
}

void Server::doAccept()
{
    acceptor.async_accept(
        [this](const asio::error_code& ec, asio::ip::tcp::socket socket)
        {
            if (!ec)
            {
                uint32_t id;
                std::shared_ptr<ServerSession> sess;
                {
                    std::lock_guard<std::mutex> lock(sessionsMutex);
                    id = nextClientId++;
                    sess = std::make_shared<ServerSession>(std::move(socket), *this, id);
                    sessions[id] = sess;
                }
                sess->start();
            }

            if (running) doAccept();
        });
}

std::vector<IncomingPacket> Server::poll()
{
    std::vector<IncomingPacket> out;
    std::lock_guard<std::mutex> lock(inboxMutex);
    out.assign(std::make_move_iterator(inbox.begin()),
               std::make_move_iterator(inbox.end()));
    inbox.clear();
    return out;
}

void Server::send(uint32_t clientId, std::vector<char> buf)
{
    std::shared_ptr<ServerSession> sess;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        auto it = sessions.find(clientId);
        if (it == sessions.end()) return;
        sess = it->second;
    }
    sess->send(std::move(buf));
}

void Server::broadcast(const std::vector<char>& buf)
{
    std::vector<std::shared_ptr<ServerSession>> all;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        all.reserve(sessions.size());
        for (auto& [id, s] : sessions) all.push_back(s);
    }
    for (auto& s : all) s->send(buf);
}

void Server::broadcastExcept(uint32_t clientId, const std::vector<char>& buf)
{
    std::vector<std::shared_ptr<ServerSession>> all;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        all.reserve(sessions.size());
        for (auto& [id, s] : sessions)
            if (id != clientId) all.push_back(s);
    }
    for (auto& s : all) s->send(buf);
}

void Server::removeSession(uint32_t clientId)
{
    std::shared_ptr<ServerSession> sess;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        auto it = sessions.find(clientId);
        if (it == sessions.end()) return;
        sess = it->second;
        sessions.erase(it);
    }
    sess->close();
}

void Server::deliver(IncomingPacket pkt)
{
    std::lock_guard<std::mutex> lock(inboxMutex);
    inbox.push_back(std::move(pkt));
}

std::vector<uint32_t> Server::clientIds()
{
    std::vector<uint32_t> ids;
    std::lock_guard<std::mutex> lock(sessionsMutex);
    ids.reserve(sessions.size());
    for (auto& [id, s] : sessions) ids.push_back(id);
    return ids;
}
