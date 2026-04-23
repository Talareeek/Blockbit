#include "../include/Command.hpp"

#include "../include/Console.hpp"
#include "../include/World.hpp"
#include "../include/Game.hpp"

#include <sstream>

std::unordered_map<std::wstring, Command> commandDatabase =
{
    {L"echo", {false, false, 
        [](std::wstring command, Console& console, Game* game, World* world)
        {
            console.writeLine(command.substr(5));
        }
    }},

    {L"exit", {false, false,
        [](std::wstring command, Console& console, Game* game, World* world)
        {
            exit(0);
        }
    }},

    {L"terminate", {false, false,
        [](std::wstring command, Console& console, Game* game, World* world)
        {
            std::terminate();
        }
    }}
};