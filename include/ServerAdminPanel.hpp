#ifndef SERVER_ADMIN_PANEL_HPP
#define SERVER_ADMIN_PANEL_HPP

#include "AdminSession.hpp"

#include <SFML/Window/Event.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

class PacketReader;

// ImGui admin panel. Talks to the server only through an AdminSession, which
// is where the "is this allowed" and "where does the data live" questions get
// answered — same panel works against a LocalAdminSession (in-process) or a
// NetworkAdminSession (remote operator).
class ServerAdminPanel
{
private:
    std::unique_ptr<AdminSession> session;

    bool open = false;

    // List state. Refreshed on open and via the Refresh button.
    std::vector<AdminEntityDescriptor> entities;
    bool list_pending = false;
    bool list_received = false;

    // Selected entity + working copy.
    std::optional<UUID> selected_entity;
    std::optional<Tag>  working_copy;
    bool                working_copy_pending = false;
    bool                working_copy_dirty   = false;
    bool                write_pending        = false;

    std::string status_line;

    // Bans / operators are read-only lists in the panel. Each has its own
    // pending/received pair so a refresh on one tab doesn't reset the other.
    std::vector<AdminBanDescriptor> bans;
    bool bans_pending  = false;
    bool bans_received = false;
    bool ban_mutation_pending = false;
    char ban_input_nickname[64] = "";
    char ban_input_reason[128]  = "";

    std::vector<std::string> operators;
    bool operators_pending  = false;
    bool operators_received = false;
    bool operator_mutation_pending = false;
    char operator_input_nickname[64] = "";

    void requestList();
    void requestBans();
    void requestOperators();
    void requestEntity(UUID id);
    void applyWorkingCopy();
    void drawEntitiesTab();
    void drawBansTab();
    void drawOperatorsTab();

public:
    explicit ServerAdminPanel(std::unique_ptr<AdminSession> session);

    void toggle();
    bool isOpen() const { return open; }

    void handleEvent(const sf::Event& event);
    void performImGui();

    // Called by ClientGameState when a PacketType::AdminResponse arrives so
    // the network session can resolve its parked callbacks.
    void onAdminResponsePacket(PacketReader& reader);
};

#endif // SERVER_ADMIN_PANEL_HPP
