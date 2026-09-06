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
#include "../include/InventoryComponent.hpp"

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

    {L"lh_entities", {false, true,
        [](std::wstring command, Console& console, Game* game, World* world)
        {
            console.writeLine(L"Entities:");

            for(auto& [uuid, entity] : world->getEntities())
            {
                std::string uuid_string = uuid.toString();
                std::wstring uuid_wstring(uuid_string.begin(), uuid_string.end());

                console.writeLine(L"   " + uuid_wstring);
            }

            console.writeLine(L"Players:");
            for(UUID uuid : world->getPlayerEntityIDs())
            {
                auto& e = world->getEntity(uuid);

                std::string uuid_string = uuid.toString();
                std::wstring uuid_wstring(uuid_string.begin(), uuid_string.end());

                std::string nickname = e.getComponent<PlayerControlledComponent>().nickname;
                console.writeLine(L"   entity " + uuid_wstring + L" (player " + std::wstring(nickname.begin(), nickname.end()) + L")");
            }
        }
    }},

    {L"lh_tp", {false, true,
        [](std::wstring command, Console& console, Game* game, World* world)
        {
            std::wistringstream stream(command);

            std::wstring trash;
            stream >> trash;

            std::wstring uuid_wstring;
            stream >> uuid_wstring;

            std::string uuid_string(uuid_wstring.begin(), uuid_wstring.end());

            UUID uuid = (uuidFromString(uuid_string).has_value()) ? uuidFromString(uuid_string).value() : throw CommandException("Non valid UUID");

            float x;
            stream >> x;

            float y;
            stream >> y;

            if(!stream) return;

            if(world->getEntity(uuid).hasComponent<TransformComponent>())
            {
                world->getEntity(uuid).getComponent<TransformComponent>().position = {x, y};
            }
        }
    }},

    {L"lh_add-velocity", {false, true,
        [](std::wstring command, Console& console, Game* game, World* world)
        {
            std::wistringstream stream(command);

            std::wstring trash;
            stream >> trash;

            std::wstring uuid_wstring;
            stream >> uuid_wstring;

            std::string uuid_string(uuid_wstring.begin(), uuid_wstring.end());

            UUID uuid = (uuidFromString(uuid_string).has_value()) ? uuidFromString(uuid_string).value() : throw CommandException("Non valid UUID");

            float x;
            stream >> x;

            float y;
            stream >> y;

            if(!stream) return;

            if(world->getEntity(uuid).hasComponent<TransformComponent>())
            {
                world->getEntity(uuid).getComponent<PhysicsComponent>().velocity += {x, y};
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

    {L"lh_spawn-ai", {false, true,
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

            Entity entity(generateUUID());

            entity.addComponent(TransformComponent{{x, y}, {1.0f, 0.75f}, sf::degrees(0.0f)});
            entity.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});
            entity.addComponent(RenderComponent{AssetManager::GameTextureID::Pig, {{0, 0}, {16, 12}}, {1.0f, 0.75f}});
            entity.addComponent(HealthComponent{100, 100});

            AIComponent ai;
            ai.personality = AIComponent::Personality::Aggressive;
            ai.state       = AIComponent::State::Idle;
            entity.addComponent(ai);

            AnimationComponent animation;

            animation.animations =
            {
                {AnimationState::Idle, AnimationClip{0, 2, 0.5f, true}},
                {AnimationState::Walking, AnimationClip{2, 2, 0.5f, true}}
            };

            animation.frameSize = {16, 12};

            entity.addComponent(animation);

            world->addEntity(std::move(entity));
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

    {L"lh_spawn-explosive", {false, true,
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

            Entity entity(generateUUID());

            entity.addComponent(TransformComponent{{x, y}, {1.0f, 0.75f}, sf::degrees(0.0f)});
            entity.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});
            entity.addComponent(RenderComponent{AssetManager::GameTextureID::Pig, {{0, 0}, {16, 12}}, {1.0f, 0.75f}});
            entity.addComponent(HealthComponent{100, 100});

            ExplosiveComponent explosive;
            explosive.force = force;
            explosive.fuseTime = fuse;
            entity.addComponent(explosive);

            AnimationComponent animation;

            animation.animations =
            {
                {AnimationState::Idle, AnimationClip{0, 2, 0.5f, true}},
                {AnimationState::Walking, AnimationClip{2, 2, 0.5f, true}}
            };

            animation.frameSize = {16, 12};

            entity.addComponent(animation);

            world->addEntity(std::move(entity));
        }
    }},

    {L"lh_spawn-mannequin", {false, true,
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
                console.writeLine(L"Usage: spawn-mannequin <x> <y>");
                return;
            }

            Entity entity(generateUUID());

            entity.addComponent(TransformComponent{{x, y}, {1.0f, 1.0f}, sf::degrees(0.0f)});
            entity.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});

            InventoryComponent inv(36);
            inv.inventory.slots[0] = {ItemID::Dynamite, 16};
            inv.inventory.slots[1] = {ItemID::Bucket, 1};
            inv.inventory.slots[2] = {ItemID::Woodcutter, 64};
            inv.inventory.slots[3] = {ItemID::Lighter, 1};
            inv.inventory.slots[4] = {ItemID::Diamond_Pickaxe, 1};
            inv.inventory.slots[5] = {ItemID::Diamond_Axe, 1};
            inv.inventory.slots[6] = {ItemID::Diamond_Shovel, 1};
            entity.addComponent(std::move(inv));
            entity.addComponent(RenderComponent{AssetManager::GameTextureID::Player, {{0, 0}, {16, 16}}, {1.0f, 1.0f}});
            entity.addComponent(HealthComponent{100, 100, false});

            world->addEntity(std::move(entity));
        }
    }}
};