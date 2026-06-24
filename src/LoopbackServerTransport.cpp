#include "../include/LoopbackServerTransport.hpp"

#include <stdexcept>

namespace
{
    void splitPacketBuffer(const std::vector<char>& buf, PacketType& outType, std::vector<char>& outPayload)
    {
        if (buf.empty())
            throw std::runtime_error("LoopbackTransport: empty packet buffer");

        outType    = static_cast<PacketType>(static_cast<uint8_t>(buf[0]));
        outPayload.assign(buf.begin() + 1, buf.end());
    }
}

// ----- LoopbackServerTransport -----

LoopbackServerTransport::LoopbackServerTransport(std::shared_ptr<LoopbackChannel> ch)
    : channel(std::move(ch))
{
}

LoopbackServerTransport::~LoopbackServerTransport()
{
    stop();
}

void LoopbackServerTransport::start()
{
    running = true;
}

void LoopbackServerTransport::stop()
{
    running = false;
}

std::vector<IncomingPacket> LoopbackServerTransport::poll()
{
    std::vector<IncomingPacket> out;
    std::lock_guard<std::mutex> lock(channel->mutex);
    out.assign(std::make_move_iterator(channel->toServer.begin()),
               std::make_move_iterator(channel->toServer.end()));
    channel->toServer.clear();
    return out;
}

void LoopbackServerTransport::send(uint32_t clientId, std::vector<char> buf)
{
    if (clientId != LoopbackChannel::LOOPBACK_CLIENT_ID) return;

    PacketType type;
    std::vector<char> payload;
    splitPacketBuffer(buf, type, payload);

    std::lock_guard<std::mutex> lock(channel->mutex);
    if (!channel->clientConnected) return;
    channel->toClient.push_back(ReceivedPacket{type, std::move(payload)});
}

void LoopbackServerTransport::broadcast(const std::vector<char>& buf)
{
    PacketType type;
    std::vector<char> payload;
    splitPacketBuffer(buf, type, payload);

    std::lock_guard<std::mutex> lock(channel->mutex);
    if (!channel->clientConnected) return;
    channel->toClient.push_back(ReceivedPacket{type, std::move(payload)});
}

void LoopbackServerTransport::broadcastExcept(uint32_t clientId, const std::vector<char>& buf)
{
    if (clientId == LoopbackChannel::LOOPBACK_CLIENT_ID) return;
    broadcast(buf);
}

std::vector<uint32_t> LoopbackServerTransport::clientIds()
{
    std::lock_guard<std::mutex> lock(channel->mutex);
    if (!channel->clientConnected) return {};
    return {LoopbackChannel::LOOPBACK_CLIENT_ID};
}

bool LoopbackServerTransport::isRunning() const
{
    return running;
}

// ----- LoopbackClientTransport -----

LoopbackClientTransport::LoopbackClientTransport(std::shared_ptr<LoopbackChannel> ch)
    : channel(std::move(ch))
{
}

LoopbackClientTransport::~LoopbackClientTransport()
{
    disconnect();
}

bool LoopbackClientTransport::connect(const std::string& /*host*/, uint16_t /*port*/)
{
    std::lock_guard<std::mutex> lock(channel->mutex);
    channel->clientConnected = true;
    connected = true;
    last_error.clear();
    return true;
}

void LoopbackClientTransport::disconnect()
{
    std::lock_guard<std::mutex> lock(channel->mutex);
    channel->clientConnected = false;
    connected = false;
}

void LoopbackClientTransport::send(std::vector<char> buf)
{
    if (!connected) return;

    PacketType type;
    std::vector<char> payload;
    splitPacketBuffer(buf, type, payload);

    std::lock_guard<std::mutex> lock(channel->mutex);
    channel->toServer.push_back(IncomingPacket{
        LoopbackChannel::LOOPBACK_CLIENT_ID,
        type,
        std::move(payload)
    });
}

std::vector<ReceivedPacket> LoopbackClientTransport::poll()
{
    std::vector<ReceivedPacket> out;
    std::lock_guard<std::mutex> lock(channel->mutex);
    out.assign(std::make_move_iterator(channel->toClient.begin()),
               std::make_move_iterator(channel->toClient.end()));
    channel->toClient.clear();
    return out;
}

bool LoopbackClientTransport::isConnected() const
{
    return connected;
}

const std::string& LoopbackClientTransport::getLastError() const
{
    return last_error;
}

// ----- factory -----

LoopbackPair makeLoopbackPair()
{
    auto channel = std::make_shared<LoopbackChannel>();
    return LoopbackPair{
        std::make_unique<LoopbackServerTransport>(channel),
        std::make_unique<LoopbackClientTransport>(channel)
    };
}
