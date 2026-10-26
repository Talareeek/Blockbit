#include "../include/Command.hpp"

#include "../include/World.hpp"
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
#include "../include/GameServer.hpp"
#include "../include/Item.hpp"

#ifndef HEADLESS_BUILD
#include "../include/Game.hpp"
#include "../include/BlockAtlas.hpp"
#endif

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    // Splits a command line into tokens honoring double-quoted substrings so
    // multi-word item names ("iron ore") survive /give parsing.
    std::vector<std::wstring> tokenize(const std::wstring& line)
    {
        std::vector<std::wstring> tokens;
        std::wstring current;
        bool in_quotes = false;

        for (wchar_t c : line)
        {
            if (c == L'"') { in_quotes = !in_quotes; continue; }

            if (!in_quotes && (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r'))
            {
                if (!current.empty()) { tokens.push_back(std::move(current)); current.clear(); }
            }
            else
            {
                current.push_back(c);
            }
        }

        if (!current.empty()) tokens.push_back(std::move(current));
        return tokens;
    }

    std::string wtoutf8(const std::wstring& ws)
    {
        // Narrow conversion — the project is ASCII-only for item names and
        // nicknames, so a byte copy is safe here.
        return std::string(ws.begin(), ws.end());
    }

    std::wstring utf8tow(const std::string& s)
    {
        return std::wstring(s.begin(), s.end());
    }

    void requireArgs(const std::vector<std::wstring>& tokens, size_t min, const char* usage)
    {
        if (tokens.size() < min) throw CommandException{usage};
    }

    uint32_t parseUInt(const std::wstring& s, const char* field)
    {
        try
        {
            size_t pos = 0;
            unsigned long long v = std::stoull(s, &pos);
            if (pos != s.size()) throw CommandException{std::string(field) + ": not an integer"};
            return static_cast<uint32_t>(v);
        }
        catch (const std::exception&)
        {
            throw CommandException{std::string(field) + ": not a valid integer"};
        }
    }

    float parseFloat(const std::wstring& s, const char* field)
    {
        try
        {
            size_t pos = 0;
            float v = std::stof(s, &pos);
            if (pos != s.size()) throw CommandException{std::string(field) + ": not a number"};
            return v;
        }
        catch (const std::exception&)
        {
            throw CommandException{std::string(field) + ": not a valid number"};
        }
    }

    // Everything after the n-th token (0-indexed), joined back with single
    // spaces — used for kick/ban reasons that may legitimately contain spaces.
    std::string joinRest(const std::vector<std::wstring>& tokens, size_t start)
    {
        std::string out;
        for (size_t i = start; i < tokens.size(); i++)
        {
            if (i > start) out += ' ';
            out += wtoutf8(tokens[i]);
        }
        return out;
    }

    std::string actingNickname(const CommandContext& ctx)
    {
        auto n = ctx.senderNickname();
        return n.empty() ? "<console>" : n;
    }

    GameServer& requireServer(CommandContext& ctx)
    {
        GameServer* s = ctx.server();
        if (!s) throw CommandException{"This command can only be run on the server."};
        return *s;
    }
}

std::unordered_map<std::wstring, Command> commandDatabase =
{
    {L"help", {false, false, false,
        [](std::wstring /*command*/, CommandContext& context)
        {
            context.reply(L"Available commands", sf::Color::Green);

            for(auto& [key, value] : commandDatabase)
            {
                context.reply(L"\t" + key, sf::Color::Green);
            }
        }
    }},

    {L"echo", {false, false, false,
        [](std::wstring command, CommandContext& context)
        {
            if (command.size() > 5) context.reply(command.substr(5));
        }
    }},

    {L"say", {false, false, true,
        [](std::wstring command, CommandContext& context)
        {
            auto tokens = tokenize(command);
            requireArgs(tokens, 2, "Usage: /say \"<message>\"");

            GameServer& server = requireServer(context);

            std::wstring message;
            for (size_t i = 1; i < tokens.size(); i++)
            {
                if (i > 1) message += L' ';
                message += tokens[i];
            }

            std::string sender = actingNickname(context);
            std::wstring nickname = (sender == "<console>") ? L"Server" : utf8tow(sender);

            server.sendChat(nickname, message);
        }
    }},

#ifndef HEADLESS_BUILD
    {L"export_block_atlas", {false, false, false,
        [](std::wstring /*command*/, CommandContext& context)
        {
            sf::Image image = BlockAtlas::getTexture().copyToImage();

            if(!image.saveToFile(getHomePath() / "Blockbit" / ("blockatlas_" + std::to_string(std::time(nullptr)) + ".png")))
            {
                context.reply(L"BlockAtlas export failed!", sf::Color::Red);
            }
            else
            {
                context.reply(L"BlockAtlas exported", sf::Color::Green);
            }
        }
    }},

    {L"max-fps", {true, false, false,
        [](std::wstring command, CommandContext& context)
        {
            Game* game = context.game();
            std::wistringstream stream(command);

            std::wstring trash;
            stream >> trash;

            int max_fps;
            stream >> max_fps;

            if(!stream) return;

            game->getWindow().setFramerateLimit(max_fps);

            context.reply(L"Framerate limit set to: " + std::to_wstring(max_fps));
        }
    }},

    {L"v-sync", {true, false, false,
        [](std::wstring command, CommandContext& context)
        {
            Game* game = context.game();
            std::wistringstream stream(command);

            std::wstring trash;
            stream >> trash;

            bool vsync;
            stream >> vsync;

            if(!stream) return;

            game->getWindow().setVerticalSyncEnabled(vsync);

            context.reply(L"V-Sync set to " + std::wstring((vsync) ? L"true" : L"false"));
        }
    }},

    {L"open", {true, false, false,
        [](std::wstring command, CommandContext& context)
        {
            auto tokens = tokenize(command);
            requireArgs(tokens, 2, "Usage: /open settings");

            const std::wstring& target = tokens[1];

            if (target == L"settings")
            {
                context.game()->getSettingsWindow().show();
                context.reply(L"Opened settings");
            }
            else
            {
                throw CommandException{"Unknown target. Usage: open settings"};
            }
        }
    }},
#endif

    {L"exit", {false, false, false,
        [](std::wstring /*command*/, CommandContext& /*context*/)
        {
            exit(0);
        }
    }},

    {L"terminate", {false, false, false,
        [](std::wstring /*command*/, CommandContext& /*context*/)
        {
            std::terminate();
        }
    }},

    //
    // ---- Gameplay commands ----
    //

    {L"tp", {false, true, false,
        [](std::wstring command, CommandContext& context)
        {
            auto tokens = tokenize(command);
            requireArgs(tokens, 3, "Usage: /tp <nickname> <nickname>  |  /tp <nickname> <x> <y>");

            GameServer& server = requireServer(context);
            World& world = *context.world();

            std::string target_nick = wtoutf8(tokens[1]);

            if (!context.senderIsOperator() && !context.senderNickname().empty()
                && target_nick != context.senderNickname())
            {
                throw CommandException{"Only operators can teleport other players."};
            }

            auto target_entity = server.findEntityByNickname(target_nick);
            if (!target_entity.has_value())
                throw CommandException{"Player not online: " + target_nick};

            Entity& target = world.getEntity(*target_entity);
            if (!target.hasComponent<TransformComponent>())
                throw CommandException{"Target player has no position."};

            sf::Vector2<double> destination;

            if (tokens.size() == 3)
            {
                std::string dest_nick = wtoutf8(tokens[2]);
                auto dest_entity = server.findEntityByNickname(dest_nick);
                if (!dest_entity.has_value())
                    throw CommandException{"Destination player not online: " + dest_nick};

                Entity& dest = world.getEntity(*dest_entity);
                if (!dest.hasComponent<TransformComponent>())
                    throw CommandException{"Destination player has no position."};

                destination = dest.getComponent<TransformComponent>().position;
            }
            else
            {
                destination.x = static_cast<double>(parseFloat(tokens[2], "x"));
                destination.y = static_cast<double>(parseFloat(tokens[3], "y"));
            }

            auto& transform = target.getComponent<TransformComponent>();
            transform.teleport(destination);
            transform.dirty = true;

            std::wostringstream msg;
            msg << L"Teleported " << utf8tow(target_nick)
                << L" to (" << destination.x << L", " << destination.y << L")";
            context.reply(msg.str());
        }
    }},

    {L"give", {false, true, true,
        [](std::wstring command, CommandContext& context)
        {
            auto tokens = tokenize(command);
            requireArgs(tokens, 3, "Usage: /give <nickname> <item-name> [count]");

            GameServer& server = requireServer(context);
            World& world = *context.world();

            std::string target_nick = wtoutf8(tokens[1]);

            auto target_entity = server.findEntityByNickname(target_nick);
            if (!target_entity.has_value())
                throw CommandException{"Player not online: " + target_nick};

            Entity& target = world.getEntity(*target_entity);
            if (!target.hasComponent<InventoryComponent>())
                throw CommandException{"Target has no inventory."};

            std::string item_name = wtoutf8(tokens[2]);
            auto item_id = findItemIdByName(item_name);
            if (!item_id.has_value())
                throw CommandException{"Unknown item: " + item_name};

            uint32_t quantity = 1;
            if (tokens.size() >= 4) quantity = parseUInt(tokens[3], "count");
            if (quantity == 0) throw CommandException{"Count must be at least 1."};

            auto& inv_component = target.getComponent<InventoryComponent>();
            bool fully_added = addItem(inv_component.inventory, *item_id, quantity);
            inv_component.dirty = true;

            std::wostringstream msg;
            msg << L"Gave " << quantity << L"x " << utf8tow(itemDatabase[*item_id].name)
                << L" to " << utf8tow(target_nick);
            if (!fully_added) msg << L" (inventory full — some items dropped)";
            context.reply(msg.str());
        }
    }},

    {L"time", {false, true, true,
        [](std::wstring command, CommandContext& context)
        {
            auto tokens = tokenize(command);
            requireArgs(tokens, 3, "Usage: /time set <0..1200>  |  /time add <seconds>");

            World& world = *context.world();

            const std::wstring& sub = tokens[1];
            float value = parseFloat(tokens[2], "value");

            if (sub == L"set")
            {
                if (value < 0.0f || value > World::DAY_CYCLE_DURATION)
                    throw CommandException{"Value for /time set must be in [0, 1200]."};

                world.dayTime = value;
            }
            else if (sub == L"add")
            {
                float next = world.dayTime + value;
                int64_t day_shift = 0;
                while (next >= World::DAY_CYCLE_DURATION) { next -= World::DAY_CYCLE_DURATION; day_shift++; }
                while (next < 0.0f)                        { next += World::DAY_CYCLE_DURATION; day_shift--; }

                world.dayTime = next;
                if (day_shift >= 0)
                    world.days += static_cast<uint64_t>(day_shift);
                else
                {
                    uint64_t back = static_cast<uint64_t>(-day_shift);
                    world.days = (back > world.days) ? 0 : world.days - back;
                }
            }
            else
            {
                throw CommandException{"Usage: /time set <value>  |  /time add <value>"};
            }

            std::wostringstream msg;
            msg << L"World time: day " << world.days << L", dayTime " << world.dayTime;
            context.reply(msg.str());
        }
    }},

    //
    // ---- Administrative commands ----
    //

    {L"op", {false, false, true,
        [](std::wstring command, CommandContext& context)
        {
            auto tokens = tokenize(command);
            requireArgs(tokens, 2, "Usage: /op <nickname>");

            GameServer& server = requireServer(context);
            std::string target = wtoutf8(tokens[1]);

            if (server.isOperator(target))
                throw CommandException{target + " is already an operator."};

            server.addOperator(target);

            std::cout << "[Server] " << actingNickname(context) << " granted operator to " << target << '\n';
            context.reply(L"Granted operator: " + tokens[1]);
        }
    }},

    {L"deop", {false, false, true,
        [](std::wstring command, CommandContext& context)
        {
            auto tokens = tokenize(command);
            requireArgs(tokens, 2, "Usage: /deop <nickname>");

            GameServer& server = requireServer(context);
            std::string target = wtoutf8(tokens[1]);

            if (!server.isOperator(target))
                throw CommandException{target + " is not an operator."};

            server.removeOperator(target);

            std::cout << "[Server] " << actingNickname(context) << " revoked operator from " << target << '\n';
            context.reply(L"Revoked operator: " + tokens[1]);
        }
    }},

    {L"kick", {false, false, true,
        [](std::wstring command, CommandContext& context)
        {
            auto tokens = tokenize(command);
            requireArgs(tokens, 2, "Usage: /kick <nickname> [reason]");

            GameServer& server = requireServer(context);
            std::string target = wtoutf8(tokens[1]);
            std::string reason = joinRest(tokens, 2);

            if (!server.kickClient(target, utf8tow(reason)))
                throw CommandException{"Player not online: " + target};

            std::cout << "[Server] " << actingNickname(context) << " kicked " << target;
            if (!reason.empty()) std::cout << ": " << reason;
            std::cout << '\n';

            context.reply(L"Kicked " + tokens[1]);
        }
    }},

    {L"ban", {false, false, true,
        [](std::wstring command, CommandContext& context)
        {
            auto tokens = tokenize(command);
            requireArgs(tokens, 2, "Usage: /ban <nickname> [reason]");

            GameServer& server = requireServer(context);
            std::string target = wtoutf8(tokens[1]);
            std::string reason = joinRest(tokens, 2);

            server.addBan(target, reason);
            // Not every banned player is currently connected; a persistent ban
            // only requires the banned.json entry. Kick if they happen to be
            // online right now.
            bool was_online = server.kickClient(target, utf8tow(reason.empty() ? std::string("banned") : reason));

            std::cout << "[Server] " << actingNickname(context) << " banned " << target;
            if (!reason.empty()) std::cout << ": " << reason;
            if (!was_online)     std::cout << " (offline)";
            std::cout << '\n';

            context.reply(L"Banned " + tokens[1]);
        }
    }},

    {L"unban", {false, false, true,
        [](std::wstring command, CommandContext& context)
        {
            auto tokens = tokenize(command);
            requireArgs(tokens, 2, "Usage: /unban <nickname>");

            GameServer& server = requireServer(context);
            std::string target = wtoutf8(tokens[1]);

            if (!server.removeBan(target))
                throw CommandException{target + " is not banned."};

            std::cout << "[Server] " << actingNickname(context) << " unbanned " << target << '\n';
            context.reply(L"Unbanned " + tokens[1]);
        }
    }},

    //
    // ---- Debug / operator-only tools (previously the lh_* set) ----
    //

    {L"entities", {false, true, true,
        [](std::wstring /*command*/, CommandContext& context)
        {
            World* world = context.world();
            context.reply(L"Entities:");

            for(auto& [uuid, entity] : world->getEntities())
            {
                std::string uuid_string = uuid.toString();
                context.reply(L"   " + utf8tow(uuid_string));
            }

            context.reply(L"Players:");
            for(UUID uuid : world->getPlayerEntityIDs())
            {
                auto& e = world->getEntity(uuid);

                std::string uuid_string = uuid.toString();
                std::string nickname = e.getComponent<PlayerControlledComponent>().nickname;

                context.reply(L"   entity " + utf8tow(uuid_string) + L" (player " + utf8tow(nickname) + L")");
            }
        }
    }},

    {L"add-velocity", {false, true, true,
        [](std::wstring command, CommandContext& context)
        {
            auto tokens = tokenize(command);
            requireArgs(tokens, 4, "Usage: /add-velocity <uuid> <x> <y>");

            World* world = context.world();

            std::string uuid_string = wtoutf8(tokens[1]);
            auto uuid_opt = uuidFromString(uuid_string);
            if (!uuid_opt.has_value()) throw CommandException{"Non valid UUID"};

            float x = parseFloat(tokens[2], "x");
            float y = parseFloat(tokens[3], "y");

            Entity& entity = world->getEntity(*uuid_opt);
            if (!entity.hasComponent<PhysicsComponent>())
                throw CommandException{"Entity has no PhysicsComponent."};

            entity.getComponent<PhysicsComponent>().velocity += sf::Vector2f(x, y);
            entity.getComponent<PhysicsComponent>().dirty = true;
        }
    }},

    {L"spawn-ai", {false, true, true,
        [](std::wstring command, CommandContext& context)
        {
            auto tokens = tokenize(command);
            requireArgs(tokens, 3, "Usage: /spawn-ai <x> <y>");

            World* world = context.world();

            float x = parseFloat(tokens[1], "x");
            float y = parseFloat(tokens[2], "y");

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

    {L"spawn-explosive", {false, true, true,
        [](std::wstring command, CommandContext& context)
        {
            auto tokens = tokenize(command);
            requireArgs(tokens, 5, "Usage: /spawn-explosive <x> <y> <force> <fuse>");

            World* world = context.world();

            float x     = parseFloat(tokens[1], "x");
            float y     = parseFloat(tokens[2], "y");
            float force = parseFloat(tokens[3], "force");
            float fuse  = parseFloat(tokens[4], "fuse");

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

    {L"noclip", {false, true, true,
        [](std::wstring command, CommandContext& context)
        {
            GameServer& server = requireServer(context);
            World& world = *context.world();

            auto tokens = tokenize(command);

            std::string target_nick;
            if (tokens.size() >= 2)
            {
                target_nick = wtoutf8(tokens[1]);

                if (!context.senderIsOperator() && target_nick != context.senderNickname())
                    throw CommandException{"Only operators can toggle noclip on other players."};
            }
            else
            {
                target_nick = context.senderNickname();
                if (target_nick.empty() || target_nick == "<console>")
                    throw CommandException{"Usage: /noclip <player>"};
            }

            auto entity_id = server.findEntityByNickname(target_nick);
            if (!entity_id.has_value())
                throw CommandException{"Player not online: " + target_nick};

            Entity& self = world.getEntity(*entity_id);
            if (!self.hasComponent<PlayerControlledComponent>() || !self.hasComponent<PhysicsComponent>())
                throw CommandException{"Target has no player/physics components."};

            auto& player = self.getComponent<PlayerControlledComponent>();
            auto& physics = self.getComponent<PhysicsComponent>();

            player.noclip = !player.noclip;
            player.jump = false;
            player.dirty = true;

            physics.isStatic = player.noclip;
            physics.isGravityActive = !player.noclip;
            physics.velocity = {0.0f, 0.0f};
            physics.force = {0.0f, 0.0f};
            physics.dirty = true;

            std::wstring state = player.noclip ? L"enabled" : L"disabled";
            context.reply(L"Noclip " + state + L" for " + utf8tow(target_nick));
        }
    }},

    {L"spawn-mannequin", {false, true, true,
        [](std::wstring command, CommandContext& context)
        {
            auto tokens = tokenize(command);
            requireArgs(tokens, 3, "Usage: /spawn-mannequin <x> <y>");

            World* world = context.world();

            float x = parseFloat(tokens[1], "x");
            float y = parseFloat(tokens[2], "y");

            Entity entity(generateUUID());

            entity.addComponent(TransformComponent{{x, y}, {0.75f, 1.0f}, sf::degrees(0.0f)});
            entity.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});

            InventoryComponent inv(36);
            inv.inventory.slots[0] = {ItemID::Dynamite, 16};
            inv.inventory.slots[1] = {ItemID::Bucket, 1};
            inv.inventory.slots[2] = {ItemID::Workbench, 64};
            inv.inventory.slots[3] = {ItemID::Lighter, 1};
            inv.inventory.slots[4] = {ItemID::Diamond_Pickaxe, 1};
            inv.inventory.slots[5] = {ItemID::Diamond_Axe, 1};
            inv.inventory.slots[6] = {ItemID::Diamond_Shovel, 1};
            entity.addComponent(std::move(inv));
            entity.addComponent(RenderComponent{AssetManager::GameTextureID::Player, {{0, 0}, {16, 16}}, {1.0f, 1.0f}});
            entity.addComponent(HealthComponent{100, 100});

            world->addEntity(std::move(entity));
        }
    }}
};
