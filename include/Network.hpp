#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <cstdint>
#include <vector>
#include <cstring>

#include <asio.hpp>

#include "Packet.hpp"

constexpr std::size_t MAX_PACKET_SIZE = 4 * 1024 * 1024;

std::vector<char> encode_buffer(const std::vector<char>& buffer);

#endif // NETWORK_HPP
