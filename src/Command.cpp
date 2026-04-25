#include "../include/Command.hpp"

#include "../include/Console.hpp"
#include "../include/World.hpp"
#include "../include/Game.hpp"
#include "../include/MainGameState.hpp"

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
    }},

    {L"entities", {false, true,
        [](std::wstring command, Console& console, Game* game, World* world)
        {
            console.writeLine(L"Entities:");

            for(auto& entity : world->getEntities())
            {
                console.writeLine(L"   " + std::to_wstring(entity.getID()));
            }

            console.writeLine(L"Player: " + std::to_wstring(world->getPlayerID()));
        }
    }},

    {L"tp", {false, true,
        [](std::wstring command, Console& console, Game* game, World* world)
        {
            std::wistringstream stream(command);

            std::wstring trash;
            stream >> trash;

            uint32_t id;
            stream >> id;

            float x;
            stream >> x;

            float y;
            stream >> y;

            if(!stream) return;

            if(entityWithID(id, *world).hasComponent<TransformComponent>())
            {
                entityWithID(id, *world).getComponent<TransformComponent>().position = {x, y};
            }
        }
    }},

    {L"add-velocity", {false, true,
        [](std::wstring command, Console& console, Game* game, World* world)
        {
            std::wistringstream stream(command);

            std::wstring trash;
            stream >> trash;

            uint32_t id;
            stream >> id;

            float x;
            stream >> x;

            float y;
            stream >> y;

            if(!stream) return;

            if(entityWithID(id, *world).hasComponent<TransformComponent>())
            {
                entityWithID(id, *world).getComponent<PhysicsComponent>().velocity += {x, y};
            }
        }
    }}
};