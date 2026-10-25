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
// Usage:
//   BlockbitServer <server_dir>              - requires an existing config.json
//   BlockbitServer <server_dir> --init       - prompt-driven first-time setup
//
// In either case, control lands in the interactive management menu first;
// the server only starts when the operator types `start`.

#include "../include/GameServer.hpp"
#include "../include/NetworkServerTransport.hpp"
#include "../include/Command.hpp"
#include "../include/World.hpp"
#include "../include/GameCommon.hpp"
#include "../include/Prefab.hpp"
#include "../include/ServerConfig.hpp"
#include "../include/ServerWorld.hpp"
#include "../include/Block.hpp"
#include "../include/Item.hpp"
#include "../include/FlatWorldGenerator.hpp"

#include <array>
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
    // ---------- ANSI terminal helpers ----------
    //
    // Auto-disabled when stdout isn't a TTY (piped/redirected) or NO_COLOR is
    // set. All colour helpers are no-ops in that case so the output stays
    // greppable.

    bool g_use_ansi = false;

    void detectAnsiSupport()
    {
        if (std::getenv("NO_COLOR")) { g_use_ansi = false; return; }
        g_use_ansi = isatty(STDOUT_FILENO) != 0;
    }

    const char* ansi(const char* code) { return g_use_ansi ? code : ""; }

    namespace c
    {
        inline const char* reset()   { return ansi("\x1b[0m");  }
        inline const char* bold()    { return ansi("\x1b[1m");  }
        inline const char* dim()     { return ansi("\x1b[2m");  }
        inline const char* red()     { return ansi("\x1b[31m"); }
        inline const char* green()   { return ansi("\x1b[32m"); }
        inline const char* yellow()  { return ansi("\x1b[33m"); }
        inline const char* blue()    { return ansi("\x1b[34m"); }
        inline const char* magenta() { return ansi("\x1b[35m"); }
        inline const char* cyan()    { return ansi("\x1b[36m"); }
        inline const char* gray()    { return ansi("\x1b[90m"); }
    }

    // Draws a two-line separator rule, e.g.
    //   ─────── TITLE ───────
    void printRule(const std::string& title = "")
    {
        const char* line = g_use_ansi ? "─" : "-";
        const int width = 60;

        if (title.empty())
        {
            for (int i = 0; i < width; ++i) std::cout << line;
            std::cout << '\n';
            return;
        }

        int pad = width - static_cast<int>(title.size()) - 2;
        if (pad < 2) pad = 2;
        int left = pad / 2;
        int right = pad - left;

        std::cout << c::gray();
        for (int i = 0; i < left; ++i) std::cout << line;
        std::cout << ' ' << c::bold() << c::cyan() << title << c::reset() << c::gray() << ' ';
        for (int i = 0; i < right; ++i) std::cout << line;
        std::cout << c::reset() << '\n';
    }

    // Length of a decorated string, ignoring any CSI ANSI escape sequences
    // (so the banner's right border lines up regardless of colouring).
    int visibleLength(const std::string& s)
    {
        int n = 0;
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '[')
            {
                i += 2;
                while (i < s.size() && s[i] != 'm') ++i;
                continue;
            }
            ++n;
        }
        return n;
    }

    void printBanner(const std::filesystem::path& server_dir, const ServerConfig& cfg)
    {
        const char* tl = g_use_ansi ? "╭" : "+";
        const char* tr = g_use_ansi ? "╮" : "+";
        const char* bl = g_use_ansi ? "╰" : "+";
        const char* br = g_use_ansi ? "╯" : "+";
        const char* h  = g_use_ansi ? "─" : "-";
        const char* v  = g_use_ansi ? "│" : "|";

        const int width = 60;

        auto hline = [&](const char* corner_l, const char* corner_r)
        {
            std::cout << c::cyan() << corner_l;
            for (int i = 0; i < width - 2; ++i) std::cout << h;
            std::cout << corner_r << c::reset() << '\n';
        };

        auto content = [&](const std::string& text)
        {
            std::cout << c::cyan() << v << c::reset() << ' ' << text;
            int visible = visibleLength(text);
            for (int i = visible; i < width - 4; ++i) std::cout << ' ';
            std::cout << ' ' << c::cyan() << v << c::reset() << '\n';
        };

        hline(tl, tr);
        content(std::string(c::bold()) + "BLOCKBIT DEDICATED SERVER" + c::reset());
        content(std::string(c::dim()) + "server dir: " + c::reset() + server_dir.string());
        content(std::string(c::dim()) + "config:     " + c::reset()
                + "world=" + c::green() + (cfg.world_name.empty() ? "<none>" : cfg.world_name) + c::reset()
                + c::dim() + "  port=" + c::reset() + std::to_string(cfg.port)
                + c::dim() + "  tick=" + c::reset() + std::to_string(cfg.tick_rate) + c::dim() + "Hz" + c::reset()
                + c::dim() + "  max=" + c::reset() + std::to_string(cfg.max_players));
        hline(bl, br);
    }

    void printPrompt(const std::string& label = "menu")
    {
        const char* arrow = g_use_ansi ? "❯" : ">";
        std::cout << c::magenta() << '[' << label << "] " << c::bold() << c::cyan() << arrow << c::reset() << ' ' << std::flush;
    }

    // ---------- commands & global state ----------

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
            "  BlockbitServer <server_dir>            Enter the management menu.\n"
            "  BlockbitServer <server_dir> --init     Create config.json via prompts, then\n"
            "                                         enter the management menu.\n"
            "\n"
            "The server directory holds config.json, worlds/, ops.json, banned.json.\n"
            "The server itself only starts once you type `start` in the menu.\n"
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

    const char* generationTypeLabel(GenerationType t)
    {
        switch (t)
        {
            case GenerationType::Flat:           return "flat";
            case GenerationType::DungeonShooter: return "dungeon_shooter";
            case GenerationType::Normal:
            default:                             return "normal";
        }
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

    // Reads a prompted line from stdin. Writes the prompt (with optional default
    // hint) to stdout coloured as a label. On EOF returns the default value.
    std::string promptLine(const std::string& prompt, const std::string& default_value)
    {
        std::cout << c::cyan() << "  " << prompt << c::reset();
        if (!default_value.empty())
        {
            std::cout << ' ' << c::dim() << "[" << default_value << "]" << c::reset();
        }
        std::cout << c::gray() << ": " << c::reset() << std::flush;

        std::string line;
        if (!std::getline(std::cin, line) || line.empty()) return default_value;
        return line;
    }

    // Numbered menu picker; returns the chosen index or defaults_idx on empty.
    int promptChoice(const std::string& prompt,
                     const std::vector<std::string>& options,
                     int default_idx)
    {
        std::cout << c::cyan() << "  " << prompt << c::reset() << '\n';
        for (size_t i = 0; i < options.size(); ++i)
        {
            bool is_default = (static_cast<int>(i) == default_idx);
            std::cout << "    " << c::yellow() << '[' << (i + 1) << "]" << c::reset()
                      << ' ' << options[i];
            if (is_default) std::cout << ' ' << c::dim() << "(default)" << c::reset();
            std::cout << '\n';
        }
        std::cout << c::gray() << "  pick 1-" << options.size()
                  << " [" << (default_idx + 1) << "]: " << c::reset() << std::flush;

        std::string line;
        if (!std::getline(std::cin, line) || line.empty()) return default_idx;

        try
        {
            int v = std::stoi(line);
            if (v >= 1 && v <= static_cast<int>(options.size())) return v - 1;
        }
        catch (...) {}

        std::cout << c::yellow() << "  invalid choice, using default" << c::reset() << '\n';
        return default_idx;
    }

    bool promptYesNo(const std::string& prompt, bool default_yes)
    {
        std::string def = default_yes ? "Y/n" : "y/N";
        std::string ans = promptLine(prompt, def);
        if (ans.empty()) return default_yes;
        char c0 = static_cast<char>(std::tolower(static_cast<unsigned char>(ans[0])));
        if (c0 == 'y') return true;
        if (c0 == 'n') return false;
        return default_yes;
    }

    // ---------- --init walk-through ----------

    bool initializeServerDir(const CliArgs& args, ServerConfig& config)
    {
        try
        {
            std::filesystem::create_directories(args.server_dir / "worlds");
        }
        catch (const std::exception& e)
        {
            std::cerr << c::red() << "[Server] Could not create " << args.server_dir
                      << ": " << e.what() << c::reset() << '\n';
            return false;
        }

        printRule("INIT");
        std::cout << c::dim() << "  Setting up a new server in " << args.server_dir << c::reset() << "\n\n";

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

        if (args.world_name.has_value()) config.world_name = *args.world_name;

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
            std::cerr << c::yellow() << "[Server] Could not seed ops/bans files: "
                      << e.what() << c::reset() << '\n';
        }

        saveServerConfig(args.server_dir, config);
        std::cout << '\n' << c::green() << "  wrote " << (args.server_dir / "config.json")
                  << c::reset() << '\n';
        return true;
    }

    // ---------- management menu ----------

    struct MenuContext
    {
        std::filesystem::path server_dir;
        ServerConfig config;
    };

    std::vector<std::string> listWorldDirs(const std::filesystem::path& server_dir)
    {
        std::vector<std::string> out;
        auto worlds_dir = server_dir / "worlds";

        std::error_code ec;
        if (!std::filesystem::exists(worlds_dir, ec)) return out;

        for (const auto& entry : std::filesystem::directory_iterator(worlds_dir, ec))
        {
            if (entry.is_directory(ec)) out.push_back(entry.path().filename().string());
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    void cmdHelp()
    {
        printRule("COMMANDS");
        auto row = [](const char* name, const char* desc)
        {
            std::cout << "  " << c::yellow() << name << c::reset();
            int pad = 22 - static_cast<int>(std::strlen(name));
            for (int i = 0; i < pad; ++i) std::cout << ' ';
            std::cout << c::dim() << desc << c::reset() << '\n';
        };
        row("worlds | list",       "list worlds in this server dir");
        row("create",              "create a new world (interactive wizard)");
        row("select <name|idx>",   "set the active world");
        row("config",              "show the current config");
        row("set <key> <value>",   "edit config: port, tick-rate, max-players,");
        row("",                    "              server-name, server-description");
        row("start",               "boot the server with the active world");
        row("help | ?",            "show this help");
        row("exit | quit",         "quit without starting");
    }

    void cmdWorlds(const MenuContext& ctx)
    {
        auto worlds = listWorldDirs(ctx.server_dir);
        printRule("WORLDS");

        if (worlds.empty())
        {
            std::cout << "  " << c::dim() << "(no worlds yet — use `create`)" << c::reset() << '\n';
            return;
        }

        for (size_t i = 0; i < worlds.size(); ++i)
        {
            bool active = (worlds[i] == ctx.config.world_name);
            std::cout << "  " << c::yellow() << '[' << (i + 1) << "]" << c::reset()
                      << ' ' << (active ? c::green() : "") << worlds[i] << c::reset();
            if (active) std::cout << ' ' << c::green() << "(active)" << c::reset();
            std::cout << '\n';
        }
    }

    void cmdConfig(const MenuContext& ctx)
    {
        const auto& c_ = ctx.config;
        printRule("CONFIG");
        auto row = [](const char* key, const std::string& val)
        {
            std::cout << "  " << c::dim() << key << c::reset();
            int pad = 22 - static_cast<int>(std::strlen(key));
            for (int i = 0; i < pad; ++i) std::cout << ' ';
            std::cout << val << '\n';
        };
        row("server_name",        c_.server_name);
        row("server_description", c_.server_description);
        row("port",               std::to_string(c_.port));
        row("tick_rate",          std::to_string(c_.tick_rate) + " Hz");
        row("max_players",        std::to_string(c_.max_players));
        row("world_name",         c_.world_name.empty() ? "<none>" : c_.world_name);

        if (c_.generation.has_value())
        {
            row("generation.type",   generationTypeLabel(c_.generation->type));
        }
    }

    bool cmdSet(MenuContext& ctx, std::istringstream& args)
    {
        std::string key, value;
        args >> key;
        std::getline(args, value);
        // trim leading whitespace from value
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());

        if (key.empty() || value.empty())
        {
            std::cout << c::yellow() << "  usage: set <key> <value>" << c::reset() << '\n';
            return false;
        }

        try
        {
            if      (key == "port")               ctx.config.port        = static_cast<uint16_t>(std::stoi(value));
            else if (key == "tick-rate" ||
                     key == "tick_rate")          ctx.config.tick_rate   = static_cast<uint16_t>(std::stoi(value));
            else if (key == "max-players" ||
                     key == "max_players")        ctx.config.max_players = static_cast<uint32_t>(std::stoul(value));
            else if (key == "server-name" ||
                     key == "server_name")        ctx.config.server_name = value;
            else if (key == "server-description"
                 ||  key == "server_description") ctx.config.server_description = value;
            else
            {
                std::cout << c::yellow() << "  unknown key: " << key << c::reset() << '\n';
                return false;
            }
        }
        catch (const std::exception& e)
        {
            std::cout << c::red() << "  bad value: " << e.what() << c::reset() << '\n';
            return false;
        }

        saveServerConfig(ctx.server_dir, ctx.config);
        std::cout << c::green() << "  updated " << key << " -> " << value << c::reset() << '\n';
        return true;
    }

    bool cmdSelect(MenuContext& ctx, std::istringstream& args)
    {
        std::string arg;
        args >> arg;
        if (arg.empty())
        {
            std::cout << c::yellow() << "  usage: select <name|idx>" << c::reset() << '\n';
            return false;
        }

        auto worlds = listWorldDirs(ctx.server_dir);
        std::string chosen;

        // try index
        try
        {
            int idx = std::stoi(arg);
            if (idx >= 1 && idx <= static_cast<int>(worlds.size()))
            {
                chosen = worlds[idx - 1];
            }
        }
        catch (...) {}

        if (chosen.empty())
        {
            for (const auto& w : worlds)
            {
                if (w == arg) { chosen = w; break; }
            }
        }

        if (chosen.empty())
        {
            std::cout << c::red() << "  no world matches `" << arg << "`" << c::reset() << '\n';
            return false;
        }

        ctx.config.world_name = chosen;
        saveServerConfig(ctx.server_dir, ctx.config);
        std::cout << c::green() << "  active world -> " << chosen << c::reset() << '\n';
        return true;
    }

    // ---------- create-world wizard ----------
    //
    // Mirrors the GUI CreateWorldState: pick a name & seed, pick normal/flat/DS,
    // then collect the mode-specific parameters.

    void runFlatLayerEditor(std::vector<FlatWorldLayer>& layers)
    {
        struct Choice { const char* label; BlockID id; };
        static constexpr std::array<Choice, 10> options{{
            {"bedrock",     BlockID::Bedrock},
            {"stone",       BlockID::Stone},
            {"dirt",        BlockID::Dirt},
            {"grass",       BlockID::Grass},
            {"sand",        BlockID::Sand},
            {"coarse dirt", BlockID::Coarse_Dirt},
            {"snow",        BlockID::Snow},
            {"cobblestone", BlockID::Cobblestone},
            {"obsidian",    BlockID::Obsidian},
            {"oak planks",  BlockID::Oak_Planks},
        }};

        auto blockLabel = [&](BlockID id) -> const char*
        {
            for (const auto& o : options) if (o.id == id) return o.label;
            return "?";
        };

        auto printLayers = [&]()
        {
            std::cout << c::dim() << "  layers (top first):" << c::reset() << '\n';
            if (layers.empty())
            {
                std::cout << "    " << c::dim() << "(none)" << c::reset() << '\n';
                return;
            }
            for (size_t i = 0; i < layers.size(); ++i)
            {
                std::cout << "    " << c::yellow() << '[' << (i + 1) << "]" << c::reset()
                          << ' ' << blockLabel(layers[i].block)
                          << c::dim() << " x " << c::reset() << layers[i].count << '\n';
            }
        };

        while (true)
        {
            printLayers();
            std::string cmd = promptLine("flat> add | del <idx> | clear | reset | done", "done");
            std::istringstream parts(cmd);
            std::string verb; parts >> verb;

            if (verb == "done" || verb.empty()) break;
            if (verb == "reset")
            {
                layers = FlatWorldProperties::defaults().layers;
                continue;
            }
            if (verb == "clear")
            {
                layers.clear();
                continue;
            }
            if (verb == "del")
            {
                int idx = 0; parts >> idx;
                if (idx >= 1 && idx <= static_cast<int>(layers.size()))
                {
                    layers.erase(layers.begin() + (idx - 1));
                }
                continue;
            }
            if (verb == "add")
            {
                std::vector<std::string> labels;
                for (const auto& o : options) labels.emplace_back(o.label);
                int pick = promptChoice("block", labels, 1);
                int count = 1;
                try { count = std::stoi(promptLine("count", "1")); }
                catch (...) { count = 1; }
                if (count < 1) count = 1;
                layers.push_back({options[pick].id, count});
                continue;
            }

            std::cout << c::yellow() << "  unknown: " << verb << c::reset() << '\n';
        }
    }

    bool runCreateWorldWizard(MenuContext& ctx)
    {
        printRule("CREATE WORLD");

        std::string name;
        while (name.empty())
        {
            name = promptLine("world name", "");
            if (name.empty())
            {
                std::cout << c::yellow() << "  name is required (empty line to cancel again to abort)" << c::reset() << '\n';
                std::string retry = promptLine("abort?", "n");
                if (!retry.empty() && (retry[0] == 'y' || retry[0] == 'Y')) return false;
            }
        }

        auto target = ctx.server_dir / "worlds" / name;
        if (std::filesystem::exists(target))
        {
            std::cout << c::red() << "  a world named `" << name << "` already exists" << c::reset() << '\n';
            return false;
        }

        std::string seed_text = promptLine("seed (blank = random)", "");
        unsigned int seed = static_cast<unsigned int>(std::rand());
        if (!seed_text.empty())
        {
            try { seed = static_cast<unsigned int>(std::stoul(seed_text)); }
            catch (...)
            {
                std::cout << c::yellow() << "  bad seed, using random" << c::reset() << '\n';
            }
        }

        int mode_idx = promptChoice(
            "generation mode",
            {"normal", "flat", "dungeon shooter"},
            0);

        GenerationProperties props = defaultGenerationProperties(GenerationType::Normal);

        if (mode_idx == 1) props.type = GenerationType::Flat;
        else if (mode_idx == 2) props.type = GenerationType::DungeonShooter;
        else props.type = GenerationType::Normal;

        if (props.type == GenerationType::Normal)
        {
            auto parseFloat = [](const std::string& s, float def) -> float
            {
                try { return std::stof(s); } catch (...) { return def; }
            };
            props.base_height  = parseFloat(promptLine("base height",  std::to_string(props.base_height)),  props.base_height);
            props.height_scale = parseFloat(promptLine("height scale", std::to_string(props.height_scale)), props.height_scale);
            props.frequency    = parseFloat(promptLine("frequency",    std::to_string(props.frequency)),    props.frequency);
            props.amplitude    = parseFloat(promptLine("amplitude",    std::to_string(props.amplitude)),    props.amplitude);
            props.persistence  = parseFloat(promptLine("persistence",  std::to_string(props.persistence)),  props.persistence);
        }
        else if (props.type == GenerationType::Flat)
        {
            if (promptYesNo("edit flat layers?", false))
            {
                runFlatLayerEditor(props.flat_layers);
            }
        }
        else
        {
            std::cout << c::dim() << "  (dungeon shooter uses only the seed)" << c::reset() << '\n';
        }

        std::cout << '\n' << c::dim() << "  generating..." << c::reset() << '\n';

        try
        {
            std::filesystem::create_directories(target);
            // Constructor saves manifest + initial spawn chunks to disk.
            ServerWorld fresh(name, target, seed, props);
        }
        catch (const std::exception& e)
        {
            std::cerr << c::red() << "  create failed: " << e.what() << c::reset() << '\n';
            std::error_code ec;
            std::filesystem::remove_all(target, ec);
            return false;
        }

        if (promptYesNo("set as active world?", true))
        {
            ctx.config.world_name = name;
            saveServerConfig(ctx.server_dir, ctx.config);
            std::cout << c::green() << "  active world -> " << name << c::reset() << '\n';
        }
        else
        {
            std::cout << c::green() << "  created " << name << c::reset() << '\n';
        }
        return true;
    }

    enum class MenuAction { Continue, Start, Exit };

    MenuAction dispatchMenuCommand(MenuContext& ctx, const std::string& raw)
    {
        std::string line = raw;
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) line.pop_back();
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) line.erase(line.begin());

        if (line.empty()) return MenuAction::Continue;

        std::istringstream parts(line);
        std::string verb;
        parts >> verb;

        if (verb == "help" || verb == "?")              { cmdHelp(); return MenuAction::Continue; }
        if (verb == "worlds" || verb == "list" || verb == "ls")
        {
            cmdWorlds(ctx);
            return MenuAction::Continue;
        }
        if (verb == "config")                           { cmdConfig(ctx); return MenuAction::Continue; }
        if (verb == "set")                              { cmdSet(ctx, parts); return MenuAction::Continue; }
        if (verb == "select")                           { cmdSelect(ctx, parts); return MenuAction::Continue; }
        if (verb == "create" || verb == "new")          { runCreateWorldWizard(ctx); return MenuAction::Continue; }
        if (verb == "exit" || verb == "quit" || verb == "q") return MenuAction::Exit;
        if (verb == "start" || verb == "run")
        {
            if (ctx.config.world_name.empty())
            {
                std::cout << c::red() << "  no active world — use `create` or `select <name>` first"
                          << c::reset() << '\n';
                return MenuAction::Continue;
            }
            return MenuAction::Start;
        }

        std::cout << c::yellow() << "  unknown command `" << verb << "` — try `help`"
                  << c::reset() << '\n';
        return MenuAction::Continue;
    }

    // Returns true to proceed to server start, false if the operator chose exit
    // (or stdin was closed).
    bool runPreServerMenu(MenuContext& ctx)
    {
        printBanner(ctx.server_dir, ctx.config);
        std::cout << '\n' << c::dim()
                  << "  type `help` to list commands, `start` to boot the server."
                  << c::reset() << '\n';

        while (true)
        {
            std::cout << '\n';
            printPrompt();

            std::string line;
            if (!std::getline(std::cin, line))
            {
                std::cout << '\n' << c::dim() << "  stdin closed, exiting." << c::reset() << '\n';
                return false;
            }

            MenuAction action = dispatchMenuCommand(ctx, line);
            if (action == MenuAction::Exit)  return false;
            if (action == MenuAction::Start) return true;
        }
    }

    // ---------- in-server command loop ----------

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
            std::cerr << c::red() << "No command found: " << raw << c::reset() << '\n';
            return;
        }

        const Command& command_data = it->second;

        if (command_data.requires_game)
        {
            std::cerr << c::yellow() << "That command is client-only." << c::reset() << '\n';
            return;
        }

        StdoutCommandSink sink(&server.getWorld(), &server);

        try
        {
            command_data.on_call(wide, sink);
        }
        catch (const CommandException& exception)
        {
            std::cerr << c::red() << exception.message << c::reset() << '\n';
        }
        catch (const std::exception& exception)
        {
            std::cerr << c::red() << "[Server] Command exception: " << exception.what()
                      << c::reset() << '\n';
        }
    }
}

int main(int argc, char* argv[])
{
    detectAnsiSupport();

    std::cout << c::dim() << "[Blockbit Dedicated Server " << __DATE__ << " " << __TIME__ << " build]"
              << c::reset() << '\n';

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
            std::cerr << c::red() << "[Server] No config.json in " << args.server_dir
                      << ". Pass --init to create a new server directory here."
                      << c::reset() << '\n';
            return 1;
        }

        auto loaded = loadServerConfig(args.server_dir);
        if (!loaded.has_value())
        {
            std::cerr << c::red() << "[Server] Could not load "
                      << (args.server_dir / "config.json") << '.' << c::reset() << '\n';
            return 1;
        }
        config = *loaded;
    }

    PrefabRegistry::init();

    MenuContext ctx{args.server_dir, std::move(config)};
    if (!runPreServerMenu(ctx)) return 0;

    config = ctx.config; // picked up any menu edits
    if (config.world_name.empty())
    {
        std::cerr << c::red() << "[Server] config.json has no world_name." << c::reset() << '\n';
        return 1;
    }

    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::unique_ptr<GameServer> server;

    try
    {
        auto world_path = args.server_dir / "worlds" / config.world_name;
        bool world_exists = std::filesystem::exists(world_path);

        auto transport = std::make_unique<NetworkServerTransport>(config.port);

        if (world_exists)
        {
            server = std::make_unique<GameServer>(args.server_dir, world_path, std::move(transport));
            std::cout << c::green() << "[Server] Loaded world '" << config.world_name
                      << "' on port " << config.port << c::reset() << '\n';
        }
        else
        {
            // Legacy path: config pointed at a world that doesn't exist on disk
            // yet. Generate it now from config.generation / config.seed, matching
            // the old one-shot behaviour.
            unsigned int seed = config.seed.value_or(static_cast<unsigned int>(std::rand()));
            GenerationProperties props = config.generation.value_or(defaultGenerationProperties(GenerationType::Normal));

            std::filesystem::create_directories(world_path);

            server = std::make_unique<GameServer>(args.server_dir, config.world_name, seed, props, std::move(transport));
            std::cout << c::green() << "[Server] Created new world '" << config.world_name
                      << "' seed=" << seed << " on port " << config.port << c::reset() << '\n';

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
        std::cerr << c::red() << "[Server] Failed to start: " << exception.what()
                  << c::reset() << '\n';
        return 1;
    }

    const float tick_step = server->getTickStep();
    const auto step_duration = std::chrono::duration<double>(tick_step);

    std::cout << c::dim() << "[Server] Tick rate: " << server->getTickRate()
              << " Hz (step " << tick_step << "s)" << c::reset() << '\n';
    std::cout << c::dim() << "[Server] Type 'stop' / Ctrl+C to shut down. "
              << "Prefix slash-commands with /, e.g. /op <nick>." << c::reset() << '\n';

    printPrompt("server");

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
            printPrompt("server");
        }
    }

    if (server->isUnrecoverable())
    {
        std::cerr << c::red() << "[Server] Shutting down after unrecoverable state."
                  << c::reset() << '\n';
    }
    else
    {
        std::cout << c::yellow() << "[Server] Shutting down." << c::reset() << '\n';
    }

    server.reset();

    std::cout << c::green() << "[Server] Clean shutdown complete." << c::reset() << '\n';
    return 0;
}
