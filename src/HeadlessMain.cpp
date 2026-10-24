// Dedicated/headless server entry point. Compiled only into BlockbitServer.
//
// Operates on a *server directory* (analogous to a Minecraft server folder):
//
//     <server_dir>/
//         config.json       - name/description/port/tick_rate/max_players/world
//         worlds/<name>/    - the active world's data
//         ops.json          - array of operator nicknames
//         banned.json       - array of {nickname, reason, banned_at}
//
// First-time setup uses `--init` with prompts for anything flags didn't
// provide. Subsequent starts just read config.json.

#include "../include/GameServer.hpp"
#include "../include/NetworkServerTransport.hpp"
#include "../include/Command.hpp"
#include "../include/World.hpp"
#include "../include/GameCommon.hpp"
#include "../include/Prefab.hpp"
#include "../include/ServerConfig.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <poll.h>
#include <unistd.h>

namespace
{
    std::atomic_bool g_running{true};

    void signalHandler(int /*signum*/)
    {
        g_running = false;
    }

    class StdoutCommandSink : public CommandContext
    {
    public:
        StdoutCommandSink(World* world, GameServer* server) : world_ptr(world), server_ptr(server) {}

        void reply(const std::wstring& line) override
        {
            std::cout << std::string(line.begin(), line.end()) << '\n';
        }

        void reply(const std::wstring& line, sf::Color /*color*/) override
        {
            std::cout << std::string(line.begin(), line.end()) << '\n';
        }

        World*      world()  override                { return world_ptr; }
        GameServer* server() override                { return server_ptr; }
        std::string senderNickname() const override  { return "<console>"; }
        bool        senderIsOperator() const override { return true; }

    private:
        World* world_ptr;
        GameServer* server_ptr;
    };

    struct CliArgs
    {
        std::filesystem::path server_dir;
        bool init = false;
        bool show_help = false;

        // Only used with --init; override whatever would otherwise be prompted.
        std::optional<std::string>   world_name;
        std::optional<uint32_t>      seed;
        std::optional<GenerationType> gen_type;
        std::optional<uint16_t>      port;
        std::optional<uint16_t>      tick_rate;
        std::optional<uint32_t>      max_players;
        std::optional<std::string>   server_name;
        std::optional<std::string>   server_description;
    };

    void printUsage()
    {
        std::cout <<
            "Blockbit dedicated server\n"
            "\n"
            "Usage:\n"
            "  BlockbitServer <server_dir> [--init] [options]\n"
            "\n"
            "The server directory holds config.json, worlds/, ops.json, banned.json.\n"
            "\n"
            "First-time setup:\n"
            "  --init                 Create the server directory structure if missing;\n"
            "                         prompt for any field not supplied via flags.\n"
            "\n"
            "Flags (override prompts under --init; ignored otherwise):\n"
            "  --world-name <name>\n"
            "  --seed <n>\n"
            "  --flat | --normal\n"
            "  --port <n>             UDP listen port (default: " << GameServer::DEFAULT_PORT << ")\n"
            "  --tick-rate <n>        Server tick rate in Hz (default: 120)\n"
            "  --max-players <n>\n"
            "  --server-name <s>\n"
            "  --server-description <s>\n";
    }

    bool parseArgs(int argc, char* argv[], CliArgs& out)
    {
        bool got_server_dir = false;

        for (int i = 1; i < argc; i++)
        {
            std::string arg = argv[i];

            auto require = [&](const char* name) -> std::optional<std::string>
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "[Server] Missing value for " << name << '\n';
                    return std::nullopt;
                }
                return std::string(argv[++i]);
            };

            if (arg == "-h" || arg == "--help")                 { out.show_help = true; return true; }
            else if (arg == "--init")                           { out.init = true; }
            else if (arg == "--world-name")
            {
                auto v = require("--world-name"); if (!v) return false; out.world_name = *v;
            }
            else if (arg == "--seed")
            {
                auto v = require("--seed"); if (!v) return false;
                try { out.seed = static_cast<uint32_t>(std::stoul(*v)); }
                catch (...) { std::cerr << "[Server] Bad --seed\n"; return false; }
            }
            else if (arg == "--flat")                           { out.gen_type = GenerationType::Flat; }
            else if (arg == "--normal")                         { out.gen_type = GenerationType::Normal; }
            else if (arg == "--port")
            {
                auto v = require("--port"); if (!v) return false;
                try { out.port = static_cast<uint16_t>(std::stoi(*v)); }
                catch (...) { std::cerr << "[Server] Bad --port\n"; return false; }
            }
            else if (arg == "--tick-rate")
            {
                auto v = require("--tick-rate"); if (!v) return false;
                try { out.tick_rate = static_cast<uint16_t>(std::stoi(*v)); }
                catch (...) { std::cerr << "[Server] Bad --tick-rate\n"; return false; }
            }
            else if (arg == "--max-players")
            {
                auto v = require("--max-players"); if (!v) return false;
                try { out.max_players = static_cast<uint32_t>(std::stoul(*v)); }
                catch (...) { std::cerr << "[Server] Bad --max-players\n"; return false; }
            }
            else if (arg == "--server-name")
            {
                auto v = require("--server-name"); if (!v) return false; out.server_name = *v;
            }
            else if (arg == "--server-description")
            {
                auto v = require("--server-description"); if (!v) return false; out.server_description = *v;
            }
            else if (!got_server_dir && !arg.empty() && arg[0] != '-')
            {
                out.server_dir = arg;
                got_server_dir = true;
            }
            else
            {
                std::cerr << "[Server] Unknown argument: " << arg << '\n';
                return false;
            }
        }

        if (!got_server_dir && !out.show_help)
        {
            std::cerr << "[Server] Missing <server_dir>.\n";
            return false;
        }
        return true;
    }

    GenerationProperties defaultGenerationProperties(GenerationType type)
    {
        GenerationProperties p;
        p.type         = type;
        p.base_height  = 55.0f;
        p.height_scale = 35.0f;
        p.frequency    = 0.035f;
        p.amplitude    = 1.0f;
        p.persistence  = 0.55f;
        return p;
    }

    std::optional<std::string> tryReadStdinLine()
    {
        static std::string buffer;

        pollfd pfd{STDIN_FILENO, POLLIN, 0};
        int ret = poll(&pfd, 1, 0);
        if (ret <= 0) return std::nullopt;
        if (!(pfd.revents & POLLIN)) return std::nullopt;

        char chunk[256];
        ssize_t n = read(STDIN_FILENO, chunk, sizeof(chunk));
        if (n <= 0) return std::nullopt;

        buffer.append(chunk, chunk + n);
        auto nl = buffer.find('\n');
        if (nl == std::string::npos) return std::nullopt;

        std::string line = buffer.substr(0, nl);
        buffer.erase(0, nl + 1);
        return line;
    }

    std::string promptLine(const std::string& prompt, const std::string& default_value)
    {
        std::cout << prompt;
        if (!default_value.empty()) std::cout << " [" << default_value << "]";
        std::cout << ": " << std::flush;

        std::string line;
        if (!std::getline(std::cin, line) || line.empty()) return default_value;
        return line;
    }

    // --init walk-through: fills in anything the flags didn't provide.
    bool initializeServerDir(const CliArgs& args, ServerConfig& config)
    {
        try
        {
            std::filesystem::create_directories(args.server_dir / "worlds");
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Server] Could not create " << args.server_dir << ": " << e.what() << '\n';
            return false;
        }

        std::cout << "[Server] Initializing new server in " << args.server_dir << '\n';

        config.server_name        = args.server_name.value_or(
            promptLine("Server name", config.server_name));
        config.server_description = args.server_description.value_or(
            promptLine("Server description", config.server_description));
        config.max_players        = args.max_players.value_or(
            static_cast<uint32_t>(std::stoul(promptLine("Max players", std::to_string(config.max_players)))));
        config.port               = args.port.value_or(
            static_cast<uint16_t>(std::stoi(promptLine("Port", std::to_string(config.port)))));
        config.tick_rate          = args.tick_rate.value_or(
            static_cast<uint16_t>(std::stoi(promptLine("Tick rate (Hz)", std::to_string(config.tick_rate)))));
        config.world_name         = args.world_name.value_or(
            promptLine("World name", "world"));

        GenerationType gt = args.gen_type.value_or(GenerationType::Normal);
        if (!args.gen_type.has_value())
        {
            std::string choice = promptLine("World type (normal/flat)", "normal");
            if (choice == "flat") gt = GenerationType::Flat;
        }

        config.generation = defaultGenerationProperties(gt);

        if (args.seed.has_value())
        {
            config.seed = *args.seed;
        }
        else
        {
            std::string s = promptLine("Seed (blank = random)", "");
            if (s.empty())
            {
                config.seed = static_cast<uint32_t>(std::rand());
            }
            else
            {
                try { config.seed = static_cast<uint32_t>(std::stoul(s)); }
                catch (...)
                {
                    std::cerr << "[Server] Bad seed, aborting.\n";
                    return false;
                }
            }
        }

        saveServerConfig(args.server_dir, config);

        // Seed empty ops.json / banned.json so operators see the files on day one.
        try
        {
            auto ops_path = args.server_dir / "ops.json";
            auto bans_path = args.server_dir / "banned.json";

            if (!std::filesystem::exists(ops_path))
            {
                std::ofstream out(ops_path);
                out << "[]";
            }
            if (!std::filesystem::exists(bans_path))
            {
                std::ofstream out(bans_path);
                out << "[]";
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Server] Could not seed ops/bans files: " << e.what() << '\n';
        }

        std::cout << "[Server] Wrote " << (args.server_dir / "config.json") << '\n';
        return true;
    }

    void processStdinCommand(GameServer& server, const std::string& line)
    {
        if (line.empty()) return;

        if (line == "stop" || line == "shutdown" || line == "quit")
        {
            g_running = false;
            return;
        }

        // Allow operators typing with or without the leading '/'.
        std::string raw = (line[0] == '/') ? line.substr(1) : line;

        std::wstring wide(raw.begin(), raw.end());

        std::wistringstream stream(wide);
        std::wstring command_name;
        stream >> command_name;

        auto it = commandDatabase.find(command_name);
        if (it == commandDatabase.end())
        {
            std::cerr << "No command found: " << raw << '\n';
            return;
        }

        const Command& command_data = it->second;

        if (command_data.requires_game)
        {
            std::cerr << "That command is client-only.\n";
            return;
        }

        StdoutCommandSink sink(&server.getWorld(), &server);

        try
        {
            command_data.on_call(wide, sink);
        }
        catch (const CommandException& exception)
        {
            std::cerr << exception.message << '\n';
        }
        catch (const std::exception& exception)
        {
            std::cerr << "[Server] Command exception: " << exception.what() << '\n';
        }
    }
}

int main(int argc, char* argv[])
{
    std::cout << "[Blockbit Dedicated Server " << __DATE__ << " " << __TIME__ << " build]\n";

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    CliArgs args;
    if (!parseArgs(argc, argv, args))
    {
        printUsage();
        return 1;
    }

    if (args.show_help)
    {
        printUsage();
        return 0;
    }

    std::error_code dir_ec;
    bool dir_exists = std::filesystem::exists(args.server_dir, dir_ec);
    bool has_config = dir_exists && std::filesystem::exists(args.server_dir / "config.json", dir_ec);

    ServerConfig config;

    if (args.init)
    {
        if (has_config)
        {
            auto existing = loadServerConfig(args.server_dir);
            if (existing.has_value()) config = *existing;
        }

        if (!initializeServerDir(args, config)) return 1;
    }
    else
    {
        if (!has_config)
        {
            std::cerr << "[Server] No config.json in " << args.server_dir
                      << ". Pass --init to create a new server directory here.\n";
            return 1;
        }

        auto loaded = loadServerConfig(args.server_dir);
        if (!loaded.has_value())
        {
            std::cerr << "[Server] Could not load " << (args.server_dir / "config.json") << ".\n";
            return 1;
        }
        config = *loaded;
    }

    if (config.world_name.empty())
    {
        std::cerr << "[Server] config.json has no world_name.\n";
        return 1;
    }

    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    PrefabRegistry::init();

    std::unique_ptr<GameServer> server;

    try
    {
        auto world_path = args.server_dir / "worlds" / config.world_name;
        bool world_exists = std::filesystem::exists(world_path);

        auto transport = std::make_unique<NetworkServerTransport>(config.port);

        if (world_exists)
        {
            server = std::make_unique<GameServer>(args.server_dir, world_path, std::move(transport));
            std::cout << "[Server] Loaded world '" << config.world_name << "' on port " << config.port << '\n';
        }
        else
        {
            unsigned int seed = config.seed.value_or(static_cast<unsigned int>(std::rand()));
            GenerationProperties props = config.generation.value_or(defaultGenerationProperties(GenerationType::Normal));

            std::filesystem::create_directories(world_path);

            server = std::make_unique<GameServer>(args.server_dir, config.world_name, seed, props, std::move(transport));
            std::cout << "[Server] Created new world '" << config.world_name
                      << "' seed=" << seed << " on port " << config.port << '\n';

            // First-start wrote config.seed if --init prompted for one. If we
            // reach here from an existing config without a seed, persist the
            // generated one so subsequent reruns don't drift.
            if (!config.seed.has_value())
            {
                config.seed = seed;
                config.generation = props;
                saveServerConfig(args.server_dir, config);
            }
        }

        server->applyServerName(config.server_name);
        server->applyServerDescription(config.server_description);
        server->applyMaxPlayers(config.max_players);
        server->applyTickRate(config.tick_rate);
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[Server] Failed to start: " << exception.what() << '\n';
        return 1;
    }

    const float tick_step = server->getTickStep();
    const auto step_duration = std::chrono::duration<double>(tick_step);

    std::cout << "[Server] Tick rate: " << server->getTickRate() << " Hz (step " << tick_step << "s)\n";
    std::cout << "[Server] Type 'stop' / Ctrl+C to shut down. Prefix slash-commands with /, e.g. /op <nick>.\n";

    auto next_tick = std::chrono::steady_clock::now();

    while (g_running && !server->isUnrecoverable())
    {
        auto now = std::chrono::steady_clock::now();
        if (now < next_tick)
        {
            std::this_thread::sleep_for(next_tick - now);
        }
        next_tick += std::chrono::duration_cast<std::chrono::steady_clock::duration>(step_duration);

        server->update(tick_step);

        while (auto line = tryReadStdinLine())
        {
            processStdinCommand(*server, *line);
            if (!g_running) break;
        }
    }

    if (server->isUnrecoverable())
    {
        std::cerr << "[Server] Shutting down after unrecoverable state.\n";
    }
    else
    {
        std::cout << "[Server] Shutting down.\n";
    }

    server.reset();

    std::cout << "[Server] Clean shutdown complete.\n";
    return 0;
}
