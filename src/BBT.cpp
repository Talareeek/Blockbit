#include "../include/BBT.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include <stdexcept>

void writeBool(std::vector<uint8_t>& buffer, bool value)
{
    buffer.push_back(value ? 1 : 0);
}

void writeInt8(std::vector<uint8_t>& buffer, int8_t value)
{
    buffer.push_back(static_cast<uint8_t>(value));
}

void writeInt16(std::vector<uint8_t>& buffer, int16_t value)
{
    uint16_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    buffer.push_back(static_cast<uint8_t>(bits & 0xFF));
    buffer.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
}

void writeInt32(std::vector<uint8_t>& buffer, int32_t value)
{
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    buffer.push_back(static_cast<uint8_t>(bits & 0xFF));
    buffer.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
}

void writeInt64(std::vector<uint8_t>& buffer, int64_t value)
{
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    for (int i = 0; i < 8; ++i)
    {
        buffer.push_back(static_cast<uint8_t>(bits & 0xFF));
        bits >>= 8;
    }
}

void writeUInt8(std::vector<uint8_t>& buffer, uint8_t value)
{
    buffer.push_back(value);
}

void writeUInt16(std::vector<uint8_t>& buffer, uint16_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void writeUInt32(std::vector<uint8_t>& buffer, uint32_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void writeUInt64(std::vector<uint8_t>& buffer, uint64_t value)
{
    for (int i = 0; i < 8; ++i)
    {
        buffer.push_back(static_cast<uint8_t>(value & 0xFF));
        value >>= 8;
    }
}

void writeFloat(std::vector<uint8_t>& buffer, float value)
{
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    writeUInt32(buffer, bits);
}

void writeDouble(std::vector<uint8_t>& buffer, double value)
{
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    writeUInt64(buffer, bits);
}

void writeString(std::vector<uint8_t>& buffer, const std::string& value)
{
    writeUInt16(buffer, static_cast<uint16_t>(value.size()));
    buffer.insert(buffer.end(), value.begin(), value.end());
}

void writeByteArray(std::vector<uint8_t>& buffer, const std::vector<uint8_t>& value)
{
    writeUInt32(buffer, static_cast<uint32_t>(value.size()));
    buffer.insert(buffer.end(), value.begin(), value.end());
}

void writeTagType(std::vector<uint8_t>& buffer, TagType tag_type)
{
    writeUInt8(buffer, static_cast<uint8_t>(tag_type));
}

bool readBool(const std::vector<uint8_t>& buffer, size_t& pos)
{
    return buffer[pos++] != 0;
}

int8_t readInt8(const std::vector<uint8_t>& buffer, size_t& pos)
{
    return static_cast<int8_t>(buffer[pos++]);
}

int16_t readInt16(const std::vector<uint8_t>& buffer, size_t& pos)
{
    uint16_t bits = readUInt16(buffer, pos);
    int16_t value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

int32_t readInt32(const std::vector<uint8_t>& buffer, size_t& pos)
{
    uint32_t bits = readUInt32(buffer, pos);
    int32_t value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

int64_t readInt64(const std::vector<uint8_t>& buffer, size_t& pos)
{
    uint64_t bits = readUInt64(buffer, pos);
    int64_t value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

uint8_t readUInt8(const std::vector<uint8_t>& buffer, size_t& pos)
{
    return buffer[pos++];
}

uint16_t readUInt16(const std::vector<uint8_t>& buffer, size_t& pos)
{
    uint16_t value = static_cast<uint16_t>(buffer[pos])
                    | static_cast<uint16_t>(buffer[pos + 1]) << 8;
    pos += 2;
    return value;
}

uint32_t readUInt32(const std::vector<uint8_t>& buffer, size_t& pos)
{
    uint32_t value = static_cast<uint32_t>(buffer[pos])
                    | static_cast<uint32_t>(buffer[pos + 1]) << 8
                    | static_cast<uint32_t>(buffer[pos + 2]) << 16
                    | static_cast<uint32_t>(buffer[pos + 3]) << 24;
    pos += 4;
    return value;
}

uint64_t readUInt64(const std::vector<uint8_t>& buffer, size_t& pos)
{
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i)
    {
        value |= static_cast<uint64_t>(buffer[pos + i]) << (8 * i);
    }
    pos += 8;
    return value;
}

float readFloat(const std::vector<uint8_t>& buffer, size_t& pos)
{
    uint32_t bits = readUInt32(buffer, pos);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

double readDouble(const std::vector<uint8_t>& buffer, size_t& pos)
{
    uint64_t bits = readUInt64(buffer, pos);
    double value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::string readString(const std::vector<uint8_t>& buffer, size_t& pos)
{
    uint16_t length = readUInt16(buffer, pos);
    std::string value(reinterpret_cast<const char*>(&buffer[pos]), length);
    pos += length;
    return value;
}

std::vector<uint8_t> readByteArray(const std::vector<uint8_t>& buffer, size_t& pos)
{
    uint32_t length = readUInt32(buffer, pos);
    std::vector<uint8_t> value(buffer.begin() + pos, buffer.begin() + pos + length);
    pos += length;
    return value;
}

Tag::Tag() : tag_type(TagType::T_END), value(std::monostate{}) {}
Tag::Tag(bool value) : tag_type(TagType::T_BOOL), value(value) {}
Tag::Tag(int8_t value) : tag_type(TagType::T_INT8), value(value) {}
Tag::Tag(int16_t value) : tag_type(TagType::T_INT16), value(value) {}
Tag::Tag(int32_t value) : tag_type(TagType::T_INT32), value(value) {}
Tag::Tag(int64_t value) : tag_type(TagType::T_INT64), value(value) {}
Tag::Tag(uint8_t value) : tag_type(TagType::T_UINT8), value(value) {}
Tag::Tag(uint16_t value) : tag_type(TagType::T_UINT16), value(value) {}
Tag::Tag(uint32_t value) : tag_type(TagType::T_UINT32), value(value) {}
Tag::Tag(uint64_t value) : tag_type(TagType::T_UINT64), value(value) {}
Tag::Tag(float value) : tag_type(TagType::T_FLOAT), value(value) {}
Tag::Tag(double value) : tag_type(TagType::T_DOUBLE), value(value) {}
Tag::Tag(const std::string& value) : tag_type(TagType::T_STRING), value(value) {}
Tag::Tag(const std::vector<uint8_t>& value) : tag_type(TagType::T_BYTEARRAY), value(value) {}
Tag::Tag(const TagList& value) : tag_type(TagType::T_LIST), value(value) {}
Tag::Tag(const TagCompound& value) : tag_type(TagType::T_COMPOUND), value(value) {}

TagType Tag::type() const
{
    return tag_type;
}

Tag& Tag::operator[](const std::string& field_name)
{
    TagCompound& compound = get<TagCompound>();
    return compound[field_name];
}

const Tag& Tag::operator[](const std::string& field_name) const
{
    const TagCompound& compound = get<TagCompound>();
    auto it = compound.find(field_name);
    if (it == compound.end())
    {
        throw std::out_of_range("Tag: lack of field with name '" + field_name + "'");
    }
    return it->second;
}

static void writeTagPayload(const Tag& tag, std::vector<uint8_t>& buffer)
{
    switch (tag.type())
    {
        case TagType::T_END:       break;
        case TagType::T_BOOL:      writeBool(buffer, tag.get<bool>()); break;
        case TagType::T_INT8:      writeInt8(buffer, tag.get<int8_t>()); break;
        case TagType::T_INT16:     writeInt16(buffer, tag.get<int16_t>()); break;
        case TagType::T_INT32:     writeInt32(buffer, tag.get<int32_t>()); break;
        case TagType::T_INT64:     writeInt64(buffer, tag.get<int64_t>()); break;
        case TagType::T_UINT8:     writeUInt8(buffer, tag.get<uint8_t>()); break;
        case TagType::T_UINT16:    writeUInt16(buffer, tag.get<uint16_t>()); break;
        case TagType::T_UINT32:    writeUInt32(buffer, tag.get<uint32_t>()); break;
        case TagType::T_UINT64:    writeUInt64(buffer, tag.get<uint64_t>()); break;
        case TagType::T_FLOAT:     writeFloat(buffer, tag.get<float>()); break;
        case TagType::T_DOUBLE:    writeDouble(buffer, tag.get<double>()); break;
        case TagType::T_STRING:    writeString(buffer, tag.get<std::string>()); break;
        case TagType::T_BYTEARRAY: writeByteArray(buffer, tag.get<std::vector<uint8_t>>()); break;

        case TagType::T_LIST:
        {
            const TagList& list = tag.get<TagList>();
            TagType elementType = list.empty() ? TagType::T_END : list.front().type();
            writeUInt8(buffer, static_cast<uint8_t>(elementType));
            writeUInt32(buffer, static_cast<uint32_t>(list.size()));
            for (const Tag& element : list)
            {
                writeTagPayload(element, buffer);
            }
            break;
        }

        case TagType::T_COMPOUND:
        {
            const TagCompound& compound = tag.get<TagCompound>();
            for (const auto& [childName, child] : compound)
            {
                child.save(buffer, childName);
            }
            writeUInt8(buffer, static_cast<uint8_t>(TagType::T_END));
            break;
        }
    }
}

void Tag::save(std::vector<uint8_t>& buffer, const std::string& name) const
{
    writeTagType(buffer, type());
    writeString(buffer, name);
    writeTagPayload(*this, buffer);
}

static Tag readTagPayload(TagType type, const std::vector<uint8_t>& buffer, size_t& pos)
{
    switch (type)
    {
        case TagType::T_END:       return Tag();
        case TagType::T_BOOL:      return Tag(readBool(buffer, pos));
        case TagType::T_INT8:      return Tag(readInt8(buffer, pos));
        case TagType::T_INT16:     return Tag(readInt16(buffer, pos));
        case TagType::T_INT32:     return Tag(readInt32(buffer, pos));
        case TagType::T_INT64:     return Tag(readInt64(buffer, pos));
        case TagType::T_UINT8:     return Tag(readUInt8(buffer, pos));
        case TagType::T_UINT16:    return Tag(readUInt16(buffer, pos));
        case TagType::T_UINT32:    return Tag(readUInt32(buffer, pos));
        case TagType::T_UINT64:    return Tag(readUInt64(buffer, pos));
        case TagType::T_FLOAT:     return Tag(readFloat(buffer, pos));
        case TagType::T_DOUBLE:    return Tag(readDouble(buffer, pos));
        case TagType::T_STRING:    return Tag(readString(buffer, pos));
        case TagType::T_BYTEARRAY: return Tag(readByteArray(buffer, pos));

        case TagType::T_LIST:
        {
            TagType elementType = static_cast<TagType>(readUInt8(buffer, pos));
            uint32_t count = readUInt32(buffer, pos);
            TagList list;
            list.reserve(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                list.push_back(readTagPayload(elementType, buffer, pos));
            }
            return Tag(list);
        }

        case TagType::T_COMPOUND:
        {
            TagCompound compound;
            while (true)
            {
                TagType childType = static_cast<TagType>(readUInt8(buffer, pos));
                if (childType == TagType::T_END) break;
                std::string childName = readString(buffer, pos);
                Tag child = readTagPayload(childType, buffer, pos);
                compound.emplace(std::move(childName), std::move(child));
            }
            return Tag(compound);
        }
    }
    throw std::runtime_error("Tag: unknown TagType during read");
}

Tag Tag::load(const std::vector<uint8_t>& buffer, size_t& position)
{
    TagType type_name = static_cast<TagType>(readUInt8(buffer, position));
    readString(buffer, position);

    return readTagPayload(type_name, buffer, position);
}

BBT::BBT(std::string name) : root_compound(), bbt_name(std::move(name)) {}

Tag& BBT::operator[](const std::string& field_name)
{
    return root_compound[field_name];
}

const Tag& BBT::operator[](const std::string& field_name) const
{
    auto it = root_compound.find(field_name);
    if (it == root_compound.end())
    {
        throw std::out_of_range("BBT: lack of field with name '" + field_name + "'");
    }
    return it->second;
}

std::string BBT::name() const
{
    return bbt_name;
}

std::vector<uint8_t> BBT::save() const
{
    std::vector<uint8_t> buffer;
    writeInt32(buffer, MAGIC_NUMBER);
    writeString(buffer, bbt_name);
    Tag(root_compound).save(buffer, bbt_name);
    return buffer;
}

BBT BBT::load(const std::vector<uint8_t> buffer)
{
    size_t position = 0;

    int32_t magic = readInt32(buffer, position);
    if (magic != MAGIC_NUMBER)
    {
        throw std::runtime_error("BBT: invalid magic number");
    }

    std::string name = readString(buffer, position);
    BBT bbt(name);

    Tag root = Tag::load(buffer, position);
    bbt.root_compound = root.get<TagCompound>();

    return bbt;
}
