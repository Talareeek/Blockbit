#ifndef SERVER_CONFIG_HPP
#define SERVER_CONFIG_HPP

#include "World.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

// Headless/dedicated server directory config. Loaded from <server_dir>/config.json
// on startup; written by HeadlessMain --init. Listen-mode servers (singleplayer /
// GUI host) don't use this file — they take their config from the world directory
// directly.
struct ServerConfig
{
    std::string server_name        = "Blockbit Server";
    std::string server_description = "A server for Blockbit game";
    uint32_t    max_players        = 20;
    uint16_t    port               = 34500;
    uint16_t    tick_rate          = 120;
    uint32_t    max_ticks_per_frame = 10;

    // Which world directory (under <server_dir>/worlds/) to boot.
    std::string world_name;

    // Only used when the chosen world doesn't exist on disk yet: generate a new
    // one with these parameters on first start. Otherwise ignored.
    std::optional<uint32_t>             seed;
    std::optional<GenerationProperties> generation;
};

// Returns std::nullopt if the file is missing or malformed; caller decides
// between re-init and abort.
std::optional<ServerConfig> loadServerConfig(const std::filesystem::path& server_dir);

void saveServerConfig(const std::filesystem::path& server_dir, const ServerConfig& config);

#endif // SERVER_CONFIG_HPP
