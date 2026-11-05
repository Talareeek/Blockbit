#include "../include/LocalAdminSession.hpp"

#include "../include/GameServer.hpp"
#include "../include/World.hpp"
#include "../include/Entity.hpp"
#include "../include/PlayerControlledComponent.hpp"

#include <algorithm>

void LocalAdminSession::listEntities(ListCallback cb)
{
    std::vector<AdminEntityDescriptor> out;
    if (!server)
    {
        cb(false, std::move(out));
        return;
    }

    auto& entities = server->getWorld().getEntities();
    out.reserve(entities.size());

    for (const auto& [id, entity] : entities)
    {
        AdminEntityDescriptor descriptor;
        descriptor.id = id;

        if (entity.hasComponent<PlayerControlledComponent>())
        {
            descriptor.label = "Player: " + entity.getComponent<PlayerControlledComponent>().nickname;
        }
        else
        {
            descriptor.label = std::to_string(entity.getComponents().size()) + " comps";
        }

        out.push_back(std::move(descriptor));
    }

    std::sort(out.begin(), out.end(), [](const AdminEntityDescriptor& a, const AdminEntityDescriptor& b)
    {
        return a.label < b.label;
    });

    cb(true, std::move(out));
}

void LocalAdminSession::readEntity(UUID id, ReadCallback cb)
{
    if (!server)
    {
        cb(false, std::nullopt);
        return;
    }
    if (!server->getWorld().doesEntityExist(id))
    {
        cb(true, std::nullopt);
        return;
    }

    cb(true, server->getWorld().getEntity(id).serialize());
}

void LocalAdminSession::listBans(ListBansCallback cb)
{
    if (!server)
    {
        cb(false, {});
        return;
    }

    std::vector<AdminBanDescriptor> out;
    for (const auto& entry : server->listBans())
    {
        AdminBanDescriptor descriptor;
        descriptor.nickname  = entry.nickname;
        descriptor.reason    = entry.reason;
        descriptor.banned_at = entry.banned_at;
        out.push_back(std::move(descriptor));
    }
    cb(true, std::move(out));
}

void LocalAdminSession::listOperators(ListOperatorsCallback cb)
{
    if (!server)
    {
        cb(false, {});
        return;
    }
    cb(true, server->listOperatorNicknames());
}

void LocalAdminSession::addBan(const std::string& nickname, const std::string& reason, WriteCallback cb)
{
    if (!server || nickname.empty())
    {
        cb(server != nullptr, false);
        return;
    }
    server->addBan(nickname, reason);
    cb(true, true);
}

void LocalAdminSession::removeBan(const std::string& nickname, WriteCallback cb)
{
    if (!server || nickname.empty())
    {
        cb(server != nullptr, false);
        return;
    }
    server->removeBan(nickname);
    cb(true, true);
}

void LocalAdminSession::addOperator(const std::string& nickname, WriteCallback cb)
{
    if (!server || nickname.empty())
    {
        cb(server != nullptr, false);
        return;
    }
    server->addOperator(nickname);
    cb(true, true);
}

void LocalAdminSession::removeOperator(const std::string& nickname, WriteCallback cb)
{
    if (!server || nickname.empty())
    {
        cb(server != nullptr, false);
        return;
    }
    server->removeOperator(nickname);
    cb(true, true);
}

void LocalAdminSession::writeEntity(UUID id, const Tag& component_compound, WriteCallback cb)
{
    if (!server)
    {
        cb(false, false);
        return;
    }
    if (!server->getWorld().doesEntityExist(id))
    {
        cb(true, false);
        return;
    }

    auto& entity = server->getWorld().getEntity(id);
    entity.applyUpdate(component_compound);

    // applyUpdate deserializes without touching dirty flags; the whole point
    // of an admin edit is for the client to see the change on the next
    // snapshot, so re-dirty every component we just overwrote.
    if (component_compound.type() == TagType::T_COMPOUND)
    {
        const TagCompound& compound = component_compound.get<TagCompound>();
        for (auto& [type_index, component] : entity.getComponents())
        {
            if (compound.find(component->name()) != compound.end())
            {
                component->dirty = true;
            }
        }
    }

    cb(true, true);
}
