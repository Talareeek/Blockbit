#ifndef NETWORK_ADMIN_SESSION_HPP
#define NETWORK_ADMIN_SESSION_HPP

#include "AdminSession.hpp"
#include "Packet.hpp"

#include <cstdint>
#include <unordered_map>
#include <variant>

class ClientTransport;

// Network-backed AdminSession: fires AdminRequest packets through the
// client's transport, parks pending callbacks by request_id, resolves them
// when the matching AdminResponse arrives.
//
// Owning class (typically ServerAdminPanel/ClientGameState) is responsible
// for calling `onResponsePacket` from the client's packet dispatch loop
// whenever a PacketType::AdminResponse comes in.
class NetworkAdminSession : public AdminSession
{
private:
    ClientTransport* transport;

    uint32_t next_request_id = 1;

    // Track authorization through responses rather than guessing up front.
    // Starts true so the UI doesn't flash a "forbidden" state before the
    // first response has had a chance to come back.
    bool authorized = true;

    struct Pending
    {
        std::variant<ListCallback,
                     ReadCallback,
                     WriteCallback,
                     ListBansCallback,
                     ListOperatorsCallback> callback;
    };
    std::unordered_map<uint32_t, Pending> pending;

    uint32_t sendRequest(AdminOp op, std::vector<uint8_t> payload);
    void resolveForbidden(Pending& pending_entry);
    void resolveNotFound(Pending& pending_entry);
    void resolveError(Pending& pending_entry);

public:
    explicit NetworkAdminSession(ClientTransport* transport) : transport(transport) {}

    bool isAuthorized() const override { return authorized; }

    void listEntities(ListCallback cb) override;
    void readEntity(UUID id, ReadCallback cb) override;
    void writeEntity(UUID id, const Tag& component_compound, WriteCallback cb) override;

    void listBans(ListBansCallback cb) override;
    void listOperators(ListOperatorsCallback cb) override;

    void addBan(const std::string& nickname, const std::string& reason, WriteCallback cb) override;
    void removeBan(const std::string& nickname, WriteCallback cb) override;
    void addOperator(const std::string& nickname, WriteCallback cb) override;
    void removeOperator(const std::string& nickname, WriteCallback cb) override;

    // Called by the owner when a PacketType::AdminResponse is received.
    void onResponsePacket(PacketReader& reader) override;
};

#endif // NETWORK_ADMIN_SESSION_HPP
