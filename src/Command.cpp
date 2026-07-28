#include "../include/Command.hpp"

#include "../include/Console.hpp"
#include "../include/World.hpp"
#include "../include/Game.hpp"
#include "../include/GameCommon.hpp"
#include "../include/Entity.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/AIComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/ExplosiveComponent.hpp"
#include "../include/PlayerControlledComponent.hpp"

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

            console.writeLine(L"Players:");
            for(uint32_t id : world->getPlayerEntityIDs())
            {
                auto& e = entityWithID(id, *world);
                std::string nickname = e.getComponent<PlayerControlledComponent>().nickname;
                console.writeLine(L"   entity " + std::to_wstring(id) + L" (player " + std::wstring(nickname.begin(), nickname.end()) + L")");
            }
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
    }},

    {L"max-fps", {true, false,
        [](std::wstring command, Console& console, Game* game, World* world)
        {
            std::wistringstream stream(command);

            std::wstring trash;
            stream >> trash;

            int max_fps;

            stream >> max_fps;

            if(!stream) return;

            game->getWindow().setFramerateLimit(max_fps);

            console.writeLine(L"Framerate limit set to: " + std::to_wstring(max_fps));
        }
    }},

    {L"spawn-ai", {false, true,
        [](std::wstring command, Console& console, Game* game, World* world)
        {
            std::wistringstream stream(command);

            std::wstring trash;
            stream >> trash;

            float x;
            stream >> x;

            float y;
            stream >> y;

            if(!stream)
            {
                console.writeLine(L"Usage: spawn-ai <x> <y>");
                return;
            }

            uint32_t id = world->getPossibleID();
            world->getEntities().emplace_back(id);

            Entity& e = entityWithID(id, *world);

            e.addComponent(TransformComponent{{x, y}, {1.0f, 0.75f}, sf::degrees(0.0f)});
            e.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});
            e.addComponent(RenderComponent{21, {{0, 0}, {16, 12}}, {1.0f, 0.75f}});
            e.addComponent(HealthComponent{100, 100});

            AIComponent ai;
            ai.personality = AIComponent::Personality::Aggressive;
            ai.state       = AIComponent::State::Idle;
            e.addComponent(ai);

            AnimationComponent animation;

            animation.animations =
            {
                {AnimationState::Idle, AnimationClip{0, 2, 0.5f, true}},
                {AnimationState::Walking, AnimationClip{2, 2, 0.5f, true}}
            };

            animation.frameSize = {16, 12};

            e.addComponent(animation);

            console.writeLine(L"Spawned AI entity " + std::to_wstring(id) + L" at (" + std::to_wstring(x) + L", " + std::to_wstring(y) + L")");
        }
    }},

    {L"v-sync", {true, false,
        [](std::wstring command, Console& console, Game* game, World* world)
        {
            std::wistringstream stream(command);

            std::wstring trash;
            stream >> trash;

            bool vsync;

            stream >> vsync;

            if(!stream) return;

            game->getWindow().setVerticalSyncEnabled(vsync);

            console.writeLine(L"V-Sync set to " + std::wstring((vsync) ? L"true" : L"false"));
        }
    }},

    {L"spawn-explosive", {false, true,
        [](std::wstring command, Console& console, Game* game, World* world)
        {
            std::wistringstream stream(command);

            std::wstring trash;
            stream >> trash;

            float x;
            stream >> x;

            float y;
            stream >> y;

            float force;
            stream >> force;

            float fuse;
            stream >> fuse;

            if(!stream)
            {
                console.writeLine(L"Usage: spawn-explosive <x> <y> <force> <fuse>");
                return;
            }

            uint32_t id = world->getPossibleID();
            world->getEntities().emplace_back(id);

            Entity& e = entityWithID(id, *world);

            e.addComponent(TransformComponent{{x, y}, {1.0f, 0.75f}, sf::degrees(0.0f)});
            e.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});
            e.addComponent(RenderComponent{21, {{0, 0}, {16, 12}}, {1.0f, 0.75f}});
            e.addComponent(HealthComponent{100, 100});

            ExplosiveComponent explosive;
            explosive.force = force;
            explosive.fuseTime = fuse;
            e.addComponent(explosive);

            AnimationComponent animation;

            animation.animations =
            {
                {AnimationState::Idle, AnimationClip{0, 2, 0.5f, true}},
                {AnimationState::Walking, AnimationClip{2, 2, 0.5f, true}}
            };

            animation.frameSize = {16, 12};

            e.addComponent(animation);

            console.writeLine(L"Spawned explosive entity " + std::to_wstring(id) + L" at (" + std::to_wstring(x) + L", " + std::to_wstring(y) + L")");
        }
    }}
};