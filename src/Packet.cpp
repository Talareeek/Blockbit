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

// ----- serialize -----

std::vector<char> serializePacket(const InitializationPacket& p)
{
    PacketWriter w(PacketType::Initialization);
    w.write(p.chunk);
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

// ----- deserialize -----
// The type byte is consumed by the network layer before these are called,
// so the reader points directly at the payload.

InitializationPacket deserializeInitialization(PacketReader& r)
{
    InitializationPacket p;
    p.chunk = r.read<Chunk>();
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
        e.id        = r.read<uint32_t>();
        e.x         = r.read<float>();
        e.y         = r.read<float>();
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

        p.entities.push_back(std::move(e));
    }

    p.dayTime = r.read<float>();
    p.days    = r.read<uint64_t>();

    return p;
}

SpawnPacket deserializeSpawn(PacketReader& r)
{
    SpawnPacket p;
    p.id = r.read<uint32_t>();
    return p;
}

DespawnPacket deserializeDespawn(PacketReader& r)
{
    DespawnPacket p;
    p.id = r.read<uint32_t>();
    return p;
}

InputPacket deserializeInput(PacketReader& r)
{
    InputPacket p;
    p.id = r.read<uint32_t>();

    uint32_t count = r.read<uint32_t>();
    if (count > 1024) throw std::runtime_error("InputPacket: input batch too large");

    p.inputs.reserve(count);
    for (uint32_t i = 0; i < count; i++) p.inputs.push_back(readInput(r));

    return p;
}
