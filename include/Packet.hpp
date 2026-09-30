#ifndef PACKET_HPP
#define PACKET_HPP

#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include <stdexcept>
#include <type_traits>

#include "Chunk.hpp"
#include "World.hpp"
#include "Input.hpp"
#include "GameCommon.hpp"
#include "View.hpp"

enum class PacketType : uint8_t
{
    Login,

    StatusRequest,
    StatusResponse,

    Initialization,
    Chunk,
    BlockUpdate,
    Snapshot,
    Track,
    StopTracking,

    Input,
    ChatMessage,
    Respawn,
    Craft,

    ClientSnapshot,

    OpenView,
    CloseView,
    SetContent,
    ViewEvent
};

struct InitializationPacket
{
    uint16_t tick_rate;
};

struct ChunkPacket
{
    Chunk chunk;
};

struct BlockUpdatePacket
{
    int x;
    int y;
    Block block;
};

struct NetEntity
{
    UUID id;

    double x;
    double y;

    float size_x;
    float size_y;

    uint32_t textureID;

    int uv_x;
    int uv_y;
    int uv_size_x;
    int uv_size_y;

    uint32_t health;
    uint32_t maxHealth;

    struct InventorySlot
    {
        uint32_t itemID;
        uint32_t quantity;
    };

    std::vector<InventorySlot> inventory;
    uint8_t selectedSlot = 0;
};

struct SnapshotPacket
{
    uint64_t tick;

    std::vector<NetEntity> entities;

    float dayTime;
    uint64_t days;
};

struct TrackPacket
{
    UUID id;
};

struct InputPacket
{
    UUID id;

    std::vector<Input> inputs;
};

struct LoginPacket
{
    std::string nickname;
};

struct StatusResponsePacket
{
    std::string name;
    std::string description;
    uint32_t players;
    uint32_t max_players;

    uint8_t icon[8192];

    Date date;
    Hour hour;
};

struct ChatMessagePacket
{
    std::wstring message;
};

struct RespawnPacket
{

};

struct StopTrackingPacket
{
    
};

struct ClientSnapshotPacket
{
    double cursor_x;
    double cursor_y;
};

struct CraftPacket
{
    ItemStack requested_craft;
};

struct OpenViewPacket
{
    ViewType type;
};

struct SetContentPacket
{
    uint16_t target;
    std::variant<ItemStack, std::string> value;
};

struct ViewEventPacket
{
    uint16_t sender;

    uint8_t action;
    std::variant<ItemStack, std::string> value;
};

class PacketWriter
{
private:

    std::vector<char> data;

public:

    PacketWriter() = default;
    explicit PacketWriter(PacketType type);

    void writeBytes(const void* src, std::size_t n);

    template<typename T>
    void write(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "PacketWriter::write requires trivially copyable T");
        writeBytes(&value, sizeof(T));
    }

    template<typename T>
    void writeOptional(const std::optional<T>& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "PacketWriter::writeOptional requires trivially copyable T");

        write<bool>(value.has_value());

        if(value.has_value())
        {
            write(value.value());
        }
    }

    void writeString(const std::string& s);
    void writeWideString(const std::wstring& s);

    void writeChunk(const Chunk& chunk);

    const std::vector<char>& buffer() const { return data; }
    std::vector<char> release() { return std::move(data); }
};

class PacketReader
{
private:

    const char* ptr;
    const char* end;

public:

    PacketReader(const char* src, std::size_t n) : ptr(src), end(src + n) {}

    void readBytes(void* dst, std::size_t n);

    template<typename T>
    T read()
    {
        static_assert(std::is_trivially_copyable_v<T>, "PacketReader::read requires trivially copyable T");
        T value;
        readBytes(&value, sizeof(T));
        return value;
    }

    template<typename T>
    std::optional<T> readOptional()
    {
        static_assert(std::is_trivially_copyable_v<T>, "PacketReader::readOptional requires trivially copyable T");

        bool has_value = read<bool>();

        if(!has_value) return std::nullopt;

        std::optional<T> optional = read<T>();

        return optional;
    }

    std::string readString();
    std::wstring readWideString();

    Chunk readChunk();

    bool eof() const { return ptr >= end; }
    std::size_t remaining() const { return static_cast<std::size_t>(end - ptr); }
};

std::vector<char> serializePlainPacket(PacketType type);

std::vector<char> serializePacket(const InitializationPacket& p);
std::vector<char> serializePacket(const ChunkPacket& p);
std::vector<char> serializePacket(const BlockUpdatePacket& p);
std::vector<char> serializePacket(const SnapshotPacket& p);
std::vector<char> serializePacket(const TrackPacket& p);
std::vector<char> serializePacket(const StopTrackingPacket& p);
std::vector<char> serializePacket(const InputPacket& p);
std::vector<char> serializePacket(const StatusResponsePacket& p);
std::vector<char> serializePacket(const LoginPacket& p);
std::vector<char> serializePacket(const ChatMessagePacket& p);
std::vector<char> serializePacket(const RespawnPacket& p);
std::vector<char> serializePacket(const ClientSnapshotPacket& p);
std::vector<char> serializePacket(const CraftPacket& p);
std::vector<char> serializePacket(const OpenViewPacket& p);
std::vector<char> serializePacket(const SetContentPacket& p);
std::vector<char> serializePacket(const ViewEventPacket& p);

InitializationPacket deserializeInitialization(PacketReader& r);
ChunkPacket deserializeChunk(PacketReader& r);
BlockUpdatePacket    deserializeBlockUpdate(PacketReader& r);
SnapshotPacket       deserializeSnapshot(PacketReader& r);
TrackPacket          deserializeTrack(PacketReader& r);
InputPacket          deserializeInput(PacketReader& r);
StatusResponsePacket deserializeStatusResponse(PacketReader& r);
LoginPacket deserializeLogin(PacketReader& r);
ChatMessagePacket deserializeChatMessage(PacketReader& r);
RespawnPacket deserializeRespawn(PacketReader& r);
ClientSnapshotPacket deserializeClientSnapshot(PacketReader& r);
CraftPacket deserializeCraft(PacketReader& r);
#endif // PACKET_HPP
