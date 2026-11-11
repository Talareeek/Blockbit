#include "../include/Registry.hpp"

bool isValidRegistryKey(const RegistryKey& key)
{
    const auto colon = key.find(':');
    if (colon == std::string::npos) return false;
    if (colon == 0 || colon == key.size() - 1) return false;

    const auto valid_ns = [](char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    };
    const auto valid_name = [](char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '/';
    };

    for (std::size_t i = 0; i < colon; ++i)
    {
        if (!valid_ns(key[i])) return false;
    }
    for (std::size_t i = colon + 1; i < key.size(); ++i)
    {
        if (!valid_name(key[i])) return false;
    }
    return true;
}
