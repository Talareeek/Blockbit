#include "../include/ServerConfig.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>

namespace
{
    std::filesystem::path configPath(const std::filesystem::path& server_dir)
    {
        return server_dir / "config.json";
    }

    const char* genTypeToString(GenerationType t)
    {
        switch (t)
        {
            case GenerationType::Flat:           return "flat";
            case GenerationType::DungeonShooter: return "dungeon_shooter";
            case GenerationType::Normal:
            default:                             return "normal";
        }
    }

    GenerationType genTypeFromString(const std::string& s)
    {
        if (s == "flat")            return GenerationType::Flat;
        if (s == "dungeon_shooter") return GenerationType::DungeonShooter;
        return GenerationType::Normal;
    }
}

std::optional<ServerConfig> loadServerConfig(const std::filesystem::path& server_dir)
{
    auto path = configPath(server_dir);

    if (!std::filesystem::exists(path)) return std::nullopt;

    try
    {
        std::ifstream file(path);
        nlohmann::json data = nlohmann::json::parse(file, nullptr, true, true);

        ServerConfig c;

        c.server_name        = data.value("server_name",        c.server_name);
        c.server_description = data.value("server_description", c.server_description);
        c.max_players        = data.value("max_players",        c.max_players);
        c.port               = data.value("port",               c.port);
        c.tick_rate          = data.value("tick_rate",          c.tick_rate);
        c.max_ticks_per_frame = data.value("max_ticks_per_frame", c.max_ticks_per_frame);
        if (c.max_ticks_per_frame == 0) c.max_ticks_per_frame = 1;
        c.world_name         = data.value("world_name",         c.world_name);

        if (data.contains("seed") && !data["seed"].is_null())
        {
            c.seed = data["seed"].get<uint32_t>();
        }

        if (data.contains("generation") && data["generation"].is_object())
        {
            const auto& g = data["generation"];
            GenerationProperties props;
            props.type         = genTypeFromString(g.value("type", std::string("normal")));
            props.base_height  = g.value("base_height",  55.0f);
            props.height_scale = g.value("height_scale", 35.0f);
            props.frequency    = g.value("frequency",    0.035f);
            props.amplitude    = g.value("amplitude",    1.0f);
            props.persistence  = g.value("persistence",  0.55f);
            c.generation = props;
        }

        return c;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Server] Failed to parse " << path << ": " << e.what() << '\n';
        return std::nullopt;
    }
}

void saveServerConfig(const std::filesystem::path& server_dir, const ServerConfig& config)
{
    try
    {
        std::filesystem::create_directories(server_dir);

        nlohmann::json data;
        data["server_name"]        = config.server_name;
        data["server_description"] = config.server_description;
        data["max_players"]        = config.max_players;
        data["port"]               = config.port;
        data["tick_rate"]          = config.tick_rate;
        data["max_ticks_per_frame"] = config.max_ticks_per_frame;
        data["world_name"]         = config.world_name;

        if (config.seed.has_value()) data["seed"] = *config.seed;
        else                         data["seed"] = nullptr;

        if (config.generation.has_value())
        {
            const auto& g = *config.generation;
            data["generation"] = {
                {"type",         genTypeToString(g.type)},
                {"base_height",  g.base_height},
                {"height_scale", g.height_scale},
                {"frequency",    g.frequency},
                {"amplitude",    g.amplitude},
                {"persistence",  g.persistence}
            };
        }

        std::ofstream file(configPath(server_dir));
        file << data.dump(4);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Server] Failed to write " << configPath(server_dir) << ": " << e.what() << '\n';
    }
}
