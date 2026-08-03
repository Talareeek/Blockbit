#include "../include/Packet.hpp"

PacketWriter::PacketWriter(PacketType type)
{
    uint8_t t = static_cast<uint8_t>(type);
    writeBytes(&t, sizeof(t));
}

void PacketWriter::writeBytes(const void* src, std::size_t n)
{
    const char* bytes = static_cast<const char*>(src);
    data.insert(data.end(), bytes, bytes + n);
}

void PacketWriter::writeString(const std::string& s)
{
    uint32_t len = static_cast<uint32_t>(s.size());
    write(len);
    writeBytes(s.data(), len);
}

void PacketReader::readBytes(void* dst, std::size_t n)
{
    if (ptr + n > end)
        throw std::runtime_error("PacketReader: out of data");
    std::memcpy(dst, ptr, n);
    ptr += n;
}

std::string PacketReader::readString()
{
    uint32_t len = read<uint32_t>();
    if (ptr + len > end)
        throw std::runtime_error("PacketReader: string out of bounds");
    std::string s(ptr, ptr + len);
    ptr += len;
    return s;
}

// Wide strings are serialized as UTF-8 on the wire so the format is
// identical regardless of whether wchar_t is 16 bits (Windows) or 32 bits
// (Linux/most Unix). Payload: uint32_t byte length, then UTF-8 bytes.

void PacketWriter::writeWideString(const std::wstring& s)
{
    std::string utf8;
    utf8.reserve(s.size());

    for (std::size_t i = 0; i < s.size(); i++)
    {
        uint32_t cp = static_cast<uint32_t>(s[i]);

        if constexpr (sizeof(wchar_t) == 2)
        {
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size())
            {
                uint32_t low = static_cast<uint32_t>(s[i + 1]);
                if (low >= 0xDC00 && low <= 0xDFFF)
                {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    i++;
                }
            }
        }

        if (cp < 0x80)
        {
            utf8.push_back(static_cast<char>(cp));
        }
        else if (cp < 0x800)
        {
            utf8.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp < 0x10000)
        {
            utf8.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            utf8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else
        {
            utf8.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            utf8.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    uint32_t len = static_cast<uint32_t>(utf8.size());
    write(len);
    writeBytes(utf8.data(), len);
}

std::wstring PacketReader::readWideString()
{
    uint32_t len = read<uint32_t>();
    if (ptr + len > end)
        throw std::runtime_error("PacketReader: wide string out of bounds");

    const char* data = ptr;
    ptr += len;

    std::wstring result;
    result.reserve(len);

    std::size_t i = 0;
    while (i < len)
    {
        uint8_t b = static_cast<uint8_t>(data[i]);
        uint32_t cp = 0;

        if (b < 0x80)
        {
            cp = b;
            i += 1;
        }
        else if ((b & 0xE0) == 0xC0)
        {
            if (i + 1 >= len) throw std::runtime_error("readWideString: truncated UTF-8");
            cp  = (b & 0x1F) << 6;
            cp |= (static_cast<uint8_t>(data[i + 1]) & 0x3F);
            i += 2;
        }
        else if ((b & 0xF0) == 0xE0)
        {
            if (i + 2 >= len) throw std::runtime_error("readWideString: truncated UTF-8");
            cp  = (b & 0x0F) << 12;
            cp |= (static_cast<uint8_t>(data[i + 1]) & 0x3F) << 6;
            cp |= (static_cast<uint8_t>(data[i + 2]) & 0x3F);
            i += 3;
        }
        else if ((b & 0xF8) == 0xF0)
        {
            if (i + 3 >= len) throw std::runtime_error("readWideString: truncated UTF-8");
            cp  = (b & 0x07) << 18;
            cp |= (static_cast<uint8_t>(data[i + 1]) & 0x3F) << 12;
            cp |= (static_cast<uint8_t>(data[i + 2]) & 0x3F) << 6;
            cp |= (static_cast<uint8_t>(data[i + 3]) & 0x3F);
            i += 4;
        }
        else
        {
            throw std::runtime_error("readWideString: invalid UTF-8 lead byte");
        }

        if constexpr (sizeof(wchar_t) == 2)
        {
            if (cp >= 0x10000)
            {
                cp -= 0x10000;
                result.push_back(static_cast<wchar_t>(0xD800 | (cp >> 10)));
                result.push_back(static_cast<wchar_t>(0xDC00 | (cp & 0x3FF)));
            }
            else
            {
                result.push_back(static_cast<wchar_t>(cp));
            }
        }
        else
        {
            result.push_back(static_cast<wchar_t>(cp));
        }
    }

    return result;
}

void PacketWriter::writeChunk(const Chunk& chunk)
{
    writeBytes(chunk.blocks, CHUNK_WIDTH * CHUNK_HEIGHT * sizeof(Block));
    write(chunk.chunk_position);
    writeBytes(chunk.climates, CHUNK_WIDTH * sizeof(Climate));
    writeBytes(chunk.biomes, CHUNK_WIDTH * sizeof(Biome));

    write(static_cast<uint32_t>(chunk.entity_ids.size()));
    
    for(auto& id : chunk.entity_ids)
    {
        write(id);
    }
}

Chunk PacketReader::readChunk()
{
    Chunk chunk;

    readBytes(chunk.blocks, CHUNK_WIDTH * CHUNK_HEIGHT * sizeof(Block));
    chunk.chunk_position = read<int>();
    readBytes(chunk.climates, CHUNK_WIDTH * sizeof(Climate));
    readBytes(chunk.biomes, CHUNK_WIDTH * sizeof(Biome));

    uint32_t size = read<uint32_t>();

    for(uint32_t i = 0; i < size; i++)
    {
        chunk.entity_ids.insert(read<UUID>());
    }

    chunk.meshDirty = true;
    chunk.generated = true;

    return chunk;
}

// ----- serialize -----

std::vector<char> serializePacket(const InitializationPacket& p)
{
    PacketWriter w(PacketType::Initialization);
    w.writeChunk(p.chunk);
    return w.release();
}

std::vector<char> serializePacket(const BlockUpdatePacket& p)
{
    PacketWriter w(PacketType::BlockUpdate);
    w.write<int32_t>(static_cast<int32_t>(p.x));
    w.write<int32_t>(static_cast<int32_t>(p.y));
    w.write(p.block);
    return w.release();
}

std::vector<char> serializePacket(const SnapshotPacket& p)
{
    PacketWriter w(PacketType::Snapshot);

    uint32_t count = static_cast<uint32_t>(p.entities.size());
    w.write(count);

    for (const auto& e : p.entities)
    {
        w.write(e.id);
        w.write(e.x);
        w.write(e.y);
        w.write(e.size_x);
        w.write(e.size_y);
        w.write(e.textureID);
        w.write<int32_t>(static_cast<int32_t>(e.uv_x));
        w.write<int32_t>(static_cast<int32_t>(e.uv_y));
        w.write<int32_t>(static_cast<int32_t>(e.uv_size_x));
        w.write<int32_t>(static_cast<int32_t>(e.uv_size_y));
        w.write(e.health);
        w.write(e.maxHealth);

        uint32_t invCount = static_cast<uint32_t>(e.inventory.size());
        w.write(invCount);
        for (const auto& slot : e.inventory)
        {
            w.write(slot.itemID);
            w.write(slot.quantity);
        }
        w.write(e.selectedSlot);
    }

    w.write(p.dayTime);
    w.write(p.days);

    return w.release();
}

std::vector<char> serializePacket(const SpawnPacket& p)
{
    PacketWriter w(PacketType::Spawn);
    w.write(p.id);
    return w.release();
}

std::vector<char> serializePacket(const DespawnPacket& p)
{
    PacketWriter w(PacketType::Despawn);
    w.write(p.id);
    return w.release();
}

static void writeInput(PacketWriter& w, const Input& in)
{
    w.write<uint8_t>(static_cast<uint8_t>(in.type));

    switch (in.type)
    {
        case InputType::MOVE:
        case InputType::USE:
        case InputType::ATTACK:
            w.write(std::get<sf::Vector2f>(in.value));
            break;
        case InputType::JUMP:
            break;
        case InputType::CHANGE_SLOT:
            w.write<uint8_t>(std::get<uint8_t>(in.value));
            break;
        case InputType::DROP:
        {
            const auto& d = std::get<DropInfo>(in.value);
            w.write(d.mousePosition);
            w.write<uint8_t>(d.fullStack ? 1 : 0);
            break;
        }
    }
}

static Input readInput(PacketReader& r)
{
    Input in{};
    in.type = static_cast<InputType>(r.read<uint8_t>());

    switch (in.type)
    {
        case InputType::MOVE:
        case InputType::USE:
        case InputType::ATTACK:
            in.value = r.read<sf::Vector2f>();
            break;
        case InputType::JUMP:
            in.value = std::monostate{};
            break;
        case InputType::CHANGE_SLOT:
            in.value = r.read<uint8_t>();
            break;
        case InputType::DROP:
        {
            DropInfo d{};
            d.mousePosition = r.read<sf::Vector2f>();
            d.fullStack = r.read<uint8_t>() != 0;
            in.value = d;
            break;
        }
        default:
            throw std::runtime_error("readInput: unknown InputType");
    }

    return in;
}

std::vector<char> serializePacket(const InputPacket& p)
{
    PacketWriter w(PacketType::Input);
    w.write(p.id);

    uint32_t count = static_cast<uint32_t>(p.inputs.size());
    w.write(count);

    for (const auto& in : p.inputs) writeInput(w, in);

    return w.release();
}

std::vector<char> serializePacket(const StatusRequestPacket& p)
{
    PacketWriter writer(PacketType::StatusRequest);
    return writer.release();
}

std::vector<char> serializePacket(const StatusResponsePacket& p)
{
    PacketWriter writer(PacketType::StatusResponse);
    writer.writeString(p.name);
    writer.writeString(p.description);
    writer.write(p.players);
    writer.write(p.max_players);
    writer.writeBytes(p.icon, 8192);

    return writer.release();
}

std::vector<char> serializePacket(const LoginPacket& p)
{
    PacketWriter writer(PacketType::Login);
    writer.writeString(p.nickname);

    return writer.release();
}

std::vector<char> serializePacket(const ChatMessagePacket& p)
{
    PacketWriter writer(PacketType::ChatMessage);

    writer.writeWideString(p.message);

    return writer.release();
}

std::vector<char> serializePacket(const RespawnPacket& p)
{
    PacketWriter writer(PacketType::Respawn);

    return writer.release();
}

// ----- deserialize -----
// The type byte is consumed by the network layer before these are called,
// so the reader points directly at the payload.

InitializationPacket deserializeInitialization(PacketReader& r)
{
    InitializationPacket p;
    p.chunk = r.readChunk();
    return p;
}

BlockUpdatePacket deserializeBlockUpdate(PacketReader& r)
{
    BlockUpdatePacket p;
    p.x = r.read<int32_t>();
    p.y = r.read<int32_t>();
    p.block = r.read<Block>();
    return p;
}

SnapshotPacket deserializeSnapshot(PacketReader& r)
{
    SnapshotPacket p;
    uint32_t count = r.read<uint32_t>();
    if (count > 100000) throw std::runtime_error("Snapshot: entity count too large");

    p.entities.reserve(count);
    for (uint32_t i = 0; i < count; i++)
    {
        NetEntity e;
        e.id        = r.read<UUID>();
        e.x         = r.read<double>();
        e.y         = r.read<double>();
        e.size_x    = r.read<float>();
        e.size_y    = r.read<float>();
        e.textureID = r.read<uint32_t>();
        e.uv_x      = r.read<int32_t>();
        e.uv_y      = r.read<int32_t>();
        e.uv_size_x = r.read<int32_t>();
        e.uv_size_y = r.read<int32_t>();
        e.health    = r.read<uint32_t>();
        e.maxHealth = r.read<uint32_t>();

        uint32_t invCount = r.read<uint32_t>();
        if (invCount > 1024) throw std::runtime_error("Snapshot: inventory size too large");

        e.inventory.resize(invCount);
        for (uint32_t j = 0; j < invCount; j++)
        {
            e.inventory[j].itemID   = r.read<uint32_t>();
            e.inventory[j].quantity = r.read<uint32_t>();
        }
        e.selectedSlot = r.read<uint8_t>();

        p.entities.push_back(std::move(e));
    }

    p.dayTime = r.read<float>();
    p.days    = r.read<uint64_t>();

    return p;
}

SpawnPacket deserializeSpawn(PacketReader& r)
{
    SpawnPacket p;
    p.id = r.read<UUID>();
    return p;
}

DespawnPacket deserializeDespawn(PacketReader& r)
{
    DespawnPacket p;
    p.id = r.read<UUID>();
    return p;
}

InputPacket deserializeInput(PacketReader& r)
{
    InputPacket p;
    p.id = r.read<UUID>();

    uint32_t count = r.read<uint32_t>();
    if (count > 1024) throw std::runtime_error("InputPacket: input batch too large");

    p.inputs.reserve(count);
    for (uint32_t i = 0; i < count; i++) p.inputs.push_back(readInput(r));

    return p;
}

StatusRequestPacket deserializeStatusRequest(PacketReader& r)
{
    return StatusRequestPacket();
}

StatusResponsePacket deserializeStatusResponse(PacketReader& r)
{
    StatusResponsePacket p;

    p.name = r.readString();
    p.description = r.readString();

    p.players = r.read<uint32_t>();
    p.max_players = r.read<uint32_t>();

    r.readBytes(p.icon, 8192);

    return p;
}

LoginPacket deserializeLogin(PacketReader& r)
{
    LoginPacket p;

    p.nickname = r.readString();

    return p;
}

ChatMessagePacket deserializeChatMessage(PacketReader& r)
{
    ChatMessagePacket p;
    p.message = r.readWideString();
    return p;
}

RespawnPacket deserializeRespawn(PacketReader& r)
{
    return RespawnPacket();
}