#include "../include/NetworkAdminSession.hpp"

#include "../include/ClientTransport.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

uint32_t NetworkAdminSession::sendRequest(AdminOp op, std::vector<uint8_t> payload)
{
    uint32_t id = next_request_id++;
    AdminRequestPacket packet{ id, op, std::move(payload) };
    if (transport && transport->isConnected())
    {
        transport->send(serializePacket(packet));
    }
    return id;
}

void NetworkAdminSession::resolveForbidden(Pending& entry)
{
    if (auto* cb = std::get_if<ListCallback>(&entry.callback))
    {
        (*cb)(false, {});
    }
    else if (auto* cb = std::get_if<ReadCallback>(&entry.callback))
    {
        (*cb)(false, std::nullopt);
    }
    else if (auto* cb = std::get_if<WriteCallback>(&entry.callback))
    {
        (*cb)(false, false);
    }
    else if (auto* cb = std::get_if<ListBansCallback>(&entry.callback))
    {
        (*cb)(false, {});
    }
    else if (auto* cb = std::get_if<ListOperatorsCallback>(&entry.callback))
    {
        (*cb)(false, {});
    }
}

void NetworkAdminSession::resolveNotFound(Pending& entry)
{
    if (auto* cb = std::get_if<ReadCallback>(&entry.callback))
    {
        (*cb)(true, std::nullopt);
    }
    else if (auto* cb = std::get_if<WriteCallback>(&entry.callback))
    {
        (*cb)(true, false);
    }
    else if (auto* cb = std::get_if<ListCallback>(&entry.callback))
    {
        (*cb)(true, {});
    }
    else if (auto* cb = std::get_if<ListBansCallback>(&entry.callback))
    {
        (*cb)(true, {});
    }
    else if (auto* cb = std::get_if<ListOperatorsCallback>(&entry.callback))
    {
        (*cb)(true, {});
    }
}

void NetworkAdminSession::resolveError(Pending& entry)
{
    if (auto* cb = std::get_if<ListCallback>(&entry.callback))
    {
        (*cb)(true, {});
    }
    else if (auto* cb = std::get_if<ReadCallback>(&entry.callback))
    {
        (*cb)(true, std::nullopt);
    }
    else if (auto* cb = std::get_if<WriteCallback>(&entry.callback))
    {
        (*cb)(true, false);
    }
    else if (auto* cb = std::get_if<ListBansCallback>(&entry.callback))
    {
        (*cb)(true, {});
    }
    else if (auto* cb = std::get_if<ListOperatorsCallback>(&entry.callback))
    {
        (*cb)(true, {});
    }
}

void NetworkAdminSession::listEntities(ListCallback cb)
{
    uint32_t id = sendRequest(AdminOp::ListEntities, {});
    pending[id] = Pending{ std::move(cb) };
}

void NetworkAdminSession::readEntity(UUID id, ReadCallback cb)
{
    std::vector<uint8_t> payload(id.bytes.begin(), id.bytes.end());
    uint32_t rid = sendRequest(AdminOp::ReadEntity, std::move(payload));
    pending[rid] = Pending{ std::move(cb) };
}

void NetworkAdminSession::writeEntity(UUID id, const Tag& component_compound, WriteCallback cb)
{
    std::vector<uint8_t> payload;
    payload.reserve(16 + 64);
    payload.insert(payload.end(), id.bytes.begin(), id.bytes.end());
    component_compound.save(payload, "");

    uint32_t rid = sendRequest(AdminOp::WriteEntity, std::move(payload));
    pending[rid] = Pending{ std::move(cb) };
}

void NetworkAdminSession::listBans(ListBansCallback cb)
{
    uint32_t id = sendRequest(AdminOp::ListBans, {});
    pending[id] = Pending{ std::move(cb) };
}

void NetworkAdminSession::listOperators(ListOperatorsCallback cb)
{
    uint32_t id = sendRequest(AdminOp::ListOperators, {});
    pending[id] = Pending{ std::move(cb) };
}

void NetworkAdminSession::addBan(const std::string& nickname, const std::string& reason, WriteCallback cb)
{
    std::vector<uint8_t> payload;
    writeString(payload, nickname);
    writeString(payload, reason);
    uint32_t rid = sendRequest(AdminOp::AddBan, std::move(payload));
    pending[rid] = Pending{ std::move(cb) };
}

void NetworkAdminSession::removeBan(const std::string& nickname, WriteCallback cb)
{
    std::vector<uint8_t> payload;
    writeString(payload, nickname);
    uint32_t rid = sendRequest(AdminOp::RemoveBan, std::move(payload));
    pending[rid] = Pending{ std::move(cb) };
}

void NetworkAdminSession::addOperator(const std::string& nickname, WriteCallback cb)
{
    std::vector<uint8_t> payload;
    writeString(payload, nickname);
    uint32_t rid = sendRequest(AdminOp::AddOperator, std::move(payload));
    pending[rid] = Pending{ std::move(cb) };
}

void NetworkAdminSession::removeOperator(const std::string& nickname, WriteCallback cb)
{
    std::vector<uint8_t> payload;
    writeString(payload, nickname);
    uint32_t rid = sendRequest(AdminOp::RemoveOperator, std::move(payload));
    pending[rid] = Pending{ std::move(cb) };
}

void NetworkAdminSession::onResponsePacket(PacketReader& reader)
{
    AdminResponsePacket response = deserializeAdminResponse(reader);

    auto it = pending.find(response.request_id);
    if (it == pending.end())
    {
        std::cerr << "[AdminSession] Unknown response id " << response.request_id << '\n';
        return;
    }

    Pending entry = std::move(it->second);
    pending.erase(it);

    if (response.status == AdminStatus::Forbidden)
    {
        authorized = false;
        resolveForbidden(entry);
        return;
    }
    if (response.status == AdminStatus::NotFound)
    {
        resolveNotFound(entry);
        return;
    }
    if (response.status != AdminStatus::Ok)
    {
        resolveError(entry);
        return;
    }

    authorized = true;

    try
    {
        switch (response.op)
        {
            case AdminOp::ListEntities:
            {
                std::vector<AdminEntityDescriptor> list;
                if (!response.payload.empty())
                {
                    size_t pos = 0;
                    uint32_t count = readUInt32(response.payload, pos);
                    list.reserve(count);
                    for (uint32_t i = 0; i < count; ++i)
                    {
                        AdminEntityDescriptor descriptor;
                        if (pos + descriptor.id.bytes.size() > response.payload.size())
                            throw std::runtime_error("AdminResponse: truncated UUID");
                        std::copy_n(response.payload.begin() + pos, descriptor.id.bytes.size(),
                                    descriptor.id.bytes.begin());
                        pos += descriptor.id.bytes.size();
                        descriptor.label = readString(response.payload, pos);
                        list.push_back(std::move(descriptor));
                    }
                }
                if (auto* cb = std::get_if<ListCallback>(&entry.callback))
                {
                    (*cb)(true, std::move(list));
                }
                break;
            }
            case AdminOp::ReadEntity:
            {
                std::optional<Tag> tag;
                if (!response.payload.empty())
                {
                    size_t pos = 0;
                    tag = Tag::load(response.payload, pos);
                }
                if (auto* cb = std::get_if<ReadCallback>(&entry.callback))
                {
                    (*cb)(true, std::move(tag));
                }
                break;
            }
            case AdminOp::WriteEntity:
            {
                if (auto* cb = std::get_if<WriteCallback>(&entry.callback))
                {
                    (*cb)(true, true);
                }
                break;
            }
            case AdminOp::ListBans:
            {
                std::vector<AdminBanDescriptor> list;
                if (!response.payload.empty())
                {
                    size_t pos = 0;
                    uint32_t count = readUInt32(response.payload, pos);
                    list.reserve(count);
                    for (uint32_t i = 0; i < count; ++i)
                    {
                        AdminBanDescriptor descriptor;
                        descriptor.nickname  = readString(response.payload, pos);
                        descriptor.reason    = readString(response.payload, pos);
                        descriptor.banned_at = readString(response.payload, pos);
                        list.push_back(std::move(descriptor));
                    }
                }
                if (auto* cb = std::get_if<ListBansCallback>(&entry.callback))
                {
                    (*cb)(true, std::move(list));
                }
                break;
            }
            case AdminOp::ListOperators:
            {
                std::vector<std::string> list;
                if (!response.payload.empty())
                {
                    size_t pos = 0;
                    uint32_t count = readUInt32(response.payload, pos);
                    list.reserve(count);
                    for (uint32_t i = 0; i < count; ++i)
                    {
                        list.push_back(readString(response.payload, pos));
                    }
                }
                if (auto* cb = std::get_if<ListOperatorsCallback>(&entry.callback))
                {
                    (*cb)(true, std::move(list));
                }
                break;
            }
            case AdminOp::AddBan:
            case AdminOp::RemoveBan:
            case AdminOp::AddOperator:
            case AdminOp::RemoveOperator:
            {
                if (auto* cb = std::get_if<WriteCallback>(&entry.callback))
                {
                    (*cb)(true, true);
                }
                break;
            }
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[AdminSession] Response decode failed: " << exception.what() << '\n';
        resolveError(entry);
    }
}
