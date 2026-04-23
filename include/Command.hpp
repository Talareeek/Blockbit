#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <functional>
#include <string>
#include <unordered_map>

class World;
class Game;
class Console;

struct Command
{
    bool requires_game;
    bool requires_world;

    std::function<void(std::wstring command, Console& console, Game* game, World* world)> on_call;
};

extern std::unordered_map<std::wstring, Command> commandDatabase;

struct CommandException
{
    std::string message;
};

#endif // COMMAND_HPP