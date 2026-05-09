#include "../include/Network.hpp"

std::vector<char> encode_buffer(const std::vector<char>& buffer)
{
    std::vector<char> encoded;

    uint16_t size = htons(buffer.size());
    
    encoded.resize(sizeof(size) + buffer.size());

    std::memcpy(encoded.data(), &size, sizeof(size));
    std::memcpy(encoded.data() + sizeof(size), buffer.data(), buffer.size());

    return encoded;
}