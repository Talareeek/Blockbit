#ifndef BBT_HPP
#define BBT_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <variant>
#include <unordered_map>

enum class TagType : uint8_t
{
    T_END = 0,
    T_BOOL,
    T_INT8,
    T_INT16,
    T_INT32,
    T_INT64,
    T_UINT8,
    T_UINT16,
    T_UINT32,
    T_UINT64,
    T_FLOAT,
    T_DOUBLE,
    T_STRING,
    T_LIST,
    T_COMPOUND,
    T_BYTEARRAY
};

extern void writeBool(std::vector<uint8_t>& buffer, bool value);
extern void writeInt8(std::vector<uint8_t>& buffer, int8_t value);
extern void writeInt16(std::vector<uint8_t>& buffer, int16_t value);
extern void writeInt32(std::vector<uint8_t>& buffer, int32_t value);
extern void writeInt64(std::vector<uint8_t>& buffer, int64_t value);
extern void writeUInt8(std::vector<uint8_t>& buffer, uint8_t value);
extern void writeUInt16(std::vector<uint8_t>& buffer, uint16_t value);
extern void writeUInt32(std::vector<uint8_t>& buffer, uint32_t value);
extern void writeUInt64(std::vector<uint8_t>& buffer, uint64_t value);
extern void writeFloat(std::vector<uint8_t>& buffer, float value);
extern void writeDouble(std::vector<uint8_t>& buffer, double value);
extern void writeString(std::vector<uint8_t>& buffer, const std::string& value);
extern void writeByteArray(std::vector<uint8_t>& buffer, const std::vector<uint8_t>& value);
extern void writeTagType(std::vector<uint8_t>& buffer, TagType type);

extern bool readBool(const std::vector<uint8_t>& buffer, size_t& pos);
extern int8_t readInt8(const std::vector<uint8_t>& buffer, size_t& pos);
extern int16_t readInt16(const std::vector<uint8_t>& buffer, size_t& pos);
extern int32_t readInt32(const std::vector<uint8_t>& buffer, size_t& pos);
extern int64_t readInt64(const std::vector<uint8_t>& buffer, size_t& pos);
extern uint8_t readUInt8(const std::vector<uint8_t>& buffer, size_t& pos);
extern uint16_t readUInt16(const std::vector<uint8_t>& buffer, size_t& pos);
extern uint32_t readUInt32(const std::vector<uint8_t>& buffer, size_t& pos);
extern uint64_t readUInt64(const std::vector<uint8_t>& buffer, size_t& pos);
extern float readFloat(const std::vector<uint8_t>& buffer, size_t& pos);
extern double readDouble(const std::vector<uint8_t>& buffer, size_t& pos);
extern std::string readString(const std::vector<uint8_t>& buffer, size_t& pos);
extern std::vector<uint8_t> readByteArray(const std::vector<uint8_t>& buffer, size_t& pos);



class Tag;

using TagList = std::vector<Tag>;

using TagCompound = std::unordered_map<std::string, Tag>;

class Tag
{
private:

    TagType tag_type;

    std::variant<
        std::monostate,
        bool,
        int8_t,
        int16_t,
        int32_t,
        int64_t,
        uint8_t,
        uint16_t,
        uint32_t,
        uint64_t,
        float,
        double,
        std::string,
        TagList,
        TagCompound,
        std::vector<uint8_t>
    > value;

public:
    Tag();
    Tag(bool value);
    Tag(int8_t value);
    Tag(int16_t value);
    Tag(int32_t value);
    Tag(int64_t value);
    Tag(uint8_t value);
    Tag(uint16_t value);
    Tag(uint32_t value);
    Tag(uint64_t value);
    Tag(float value);
    Tag(double value);
    Tag(const std::string& value);
    Tag(const std::vector<uint8_t>& value);
    Tag(const TagList& value);
    Tag(const TagCompound& value);

    template<typename T>
    const T& get() const
    {
        return std::get<T>(value);
    }

    template<typename T>
    T& get()
    {
        return std::get<T>(value);
    }

    Tag& operator[](const std::string& field_name);
    const Tag& operator[](const std::string& field_name) const;

    void save(std::vector<uint8_t>& buffer, const std::string& name) const;
    static Tag load(const std::vector<uint8_t>& buffer, size_t& position);

    TagType type() const;
};

class BBT
{
private:

    TagCompound root_compound;

    std::string bbt_name;

    static constexpr int MAGIC_NUMBER = 1;

public:

    BBT(std::string name);

    Tag& operator[](const std::string& field_name);
    const Tag& operator[](const std::string& field_name) const;

    std::string name() const;

    std::vector<uint8_t> save() const;
    static BBT load(const std::vector<uint8_t> buffer);
};

#endif // BBT_HPP