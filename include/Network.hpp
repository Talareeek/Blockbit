#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <cstdint>
#include <vector>
#include <cstring>

#include <asio.hpp>

std::vector<char> encode_buffer(const std::vector<char>& buffer);

#endif // NETWORK_HPP