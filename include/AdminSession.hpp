#ifndef ADMIN_SESSION_HPP
#define ADMIN_SESSION_HPP

#include "UUID.hpp"
#include "BBT.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

class PacketReader;

struct AdminEntityDescriptor
{
    UUID id;
    std::string label;
};

struct AdminBanDescriptor
{
    std::string nickname;
    std::string reason;
    std::string banned_at;
};

// Backend-agnostic handle onto a running GameServer's admin surface. Every
// operation is asynchronous because the network-backed implementation needs
// at least one round-trip; the local implementation just fires the callback
// synchronously. The three callback aliases below are also the contract
// NetworkAdminSession parks under each in-flight request_id, so match them
// exactly when adding new operations.
class AdminSession
{
public:
    using ListCallback          = std::function<void(bool /*authorized*/, std::vector<AdminEntityDescriptor>)>;
    using ReadCallback          = std::function<void(bool /*authorized*/, std::optional<Tag>)>;
    using WriteCallback         = std::function<void(bool /*authorized*/, bool /*ok*/)>;
    using ListBansCallback      = std::function<void(bool /*authorized*/, std::vector<AdminBanDescriptor>)>;
    using ListOperatorsCallback = std::function<void(bool /*authorized*/, std::vector<std::string>)>;

    virtual ~AdminSession() = default;

    // Known-authorized hint for the UI. Local sessions always return true;
    // network sessions start true-optimistic and flip to false on the first
    // Forbidden response so the panel can show a clear message.
    virtual bool isAuthorized() const = 0;

    virtual void listEntities(ListCallback cb) = 0;
    virtual void readEntity(UUID id, ReadCallback cb) = 0;
    virtual void writeEntity(UUID id, const Tag& component_compound, WriteCallback cb) = 0;

    virtual void listBans(ListBansCallback cb) = 0;
    virtual void listOperators(ListOperatorsCallback cb) = 0;

    // Ban / operator mutations. WriteCallback reports (authorized, ok). "ok"
    // means the state change applied (or already matched); false is reserved
    // for cases like empty nickname or server unavailable.
    virtual void addBan(const std::string& nickname, const std::string& reason, WriteCallback cb) = 0;
    virtual void removeBan(const std::string& nickname, WriteCallback cb) = 0;
    virtual void addOperator(const std::string& nickname, WriteCallback cb) = 0;
    virtual void removeOperator(const std::string& nickname, WriteCallback cb) = 0;

    // Drain response packets / expire old callbacks. Noop for local sessions.
    virtual void update() {}

    // Fed by the client's packet dispatch when a PacketType::AdminResponse
    // arrives. Local sessions ignore this; network sessions decode the
    // response and resolve the parked callback.
    virtual void onResponsePacket(PacketReader&) {}
};

#endif // ADMIN_SESSION_HPP
