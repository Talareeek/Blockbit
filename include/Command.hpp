#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <functional>
#include <string>
#include <unordered_map>

#include <SFML/Graphics/Color.hpp>

class World;
class Game;
class Console;
class GameServer;

// Abstract sink for whatever runs a command. Three concrete implementations:
// client Console (writes into the ImGui console window), server stdout (headless),
// and network reply (buffers back as ChatMessagePacket to the issuing player).
class CommandContext
{
public:
    virtual ~CommandContext() = default;

    virtual void reply(const std::wstring& line) = 0;
    virtual void reply(const std::wstring& line, sf::Color color) = 0;

    // Nullable: present on client (Listen/local), always null on headless/dedicated.
    virtual Game* game() { return nullptr; }

    // Nullable: null when the command is issued pre-world.
    virtual World* world() { return nullptr; }

    // Nullable: null on client-local console. Admin commands need this to
    // mutate operator/ban lists and kick players.
    virtual GameServer* server() { return nullptr; }

    // Sender nickname where meaningful (remote network command). Empty for client
    // local console or headless stdin.
    virtual std::string senderNickname() const { return {}; }

    // Whether the sender has operator rights. Default true for trusted sinks
    // (headless stdin, client local console). Network sink checks the ops list.
    virtual bool senderIsOperator() const { return true; }
};

struct Command
{
    bool requires_game;
    bool requires_world;
    bool requires_operator;

    std::function<void(std::wstring command, CommandContext& context)> on_call;
};

extern std::unordered_map<std::wstring, Command> commandDatabase;

struct CommandException
{
    std::string message;
};

#endif // COMMAND_HPP
