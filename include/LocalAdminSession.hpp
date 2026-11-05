#ifndef LOCAL_ADMIN_SESSION_HPP
#define LOCAL_ADMIN_SESSION_HPP

#include "AdminSession.hpp"

class GameServer;

// In-process AdminSession: talks straight to a GameServer the host owns.
// Callbacks fire synchronously inside each call — the panel can rely on
// data being present immediately after the method returns.
class LocalAdminSession : public AdminSession
{
private:
    GameServer* server;

public:
    explicit LocalAdminSession(GameServer* server) : server(server) {}

    bool isAuthorized() const override { return server != nullptr; }

    void listEntities(ListCallback cb) override;
    void readEntity(UUID id, ReadCallback cb) override;
    void writeEntity(UUID id, const Tag& component_compound, WriteCallback cb) override;

    void listBans(ListBansCallback cb) override;
    void listOperators(ListOperatorsCallback cb) override;

    void addBan(const std::string& nickname, const std::string& reason, WriteCallback cb) override;
    void removeBan(const std::string& nickname, WriteCallback cb) override;
    void addOperator(const std::string& nickname, WriteCallback cb) override;
    void removeOperator(const std::string& nickname, WriteCallback cb) override;
};

#endif // LOCAL_ADMIN_SESSION_HPP
