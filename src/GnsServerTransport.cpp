#include "../include/GnsServerTransport.hpp"

#include <steam/isteamnetworkingsockets.h>

#include <atomic>
#include <cstring>
#include <iostream>
#include <mutex>

namespace
{
    // One-time GameNetworkingSockets_Init, process-wide. Shared by server
    // and client transports. Kept lazy so --legacy runs (asio TCP) don't
    // pay the init cost.
    std::once_flag g_init_flag;
    std::atomic<bool> g_init_ok{false};

    void gnsEnsureInit()
    {
        std::call_once(g_init_flag, []()
        {
            SteamNetworkingErrMsg err;
            if (!GameNetworkingSockets_Init(nullptr, err))
            {
                std::cerr << "[GNS] init failed: " << err << '\n';
                return;
            }
            g_init_ok = true;
        });
    }

    // Single-instance trampoline: at most one server transport lives per
    // process (listen-server in the client exe OR dedicated server, never
    // both). The config-driven per-socket callback has no userdata param,
    // so a static pointer is the simplest route.
    GnsServerTransport* g_server_instance = nullptr;
}

GnsServerTransport::GnsServerTransport(uint16_t p) : port(p)
{
    gnsEnsureInit();
}

GnsServerTransport::~GnsServerTransport()
{
    stop();
}

void GnsServerTransport::staticStatusCallback(SteamNetConnectionStatusChangedCallback_t* info)
{
    if (g_server_instance) g_server_instance->onConnectionStatusChanged(info);
}

void GnsServerTransport::onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info)
{
    switch (info->m_info.m_eState)
    {
        case k_ESteamNetworkingConnectionState_None:
            break;

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        {
            uint32_t client_id = 0;
            bool found = false;
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                auto it = conn_to_id.find(info->m_hConn);
                if (it != conn_to_id.end())
                {
                    client_id = it->second;
                    conn_to_id.erase(it);
                    id_to_conn.erase(client_id);
                    found = true;
                }
            }
            if (found)
            {
                std::cerr << "[GNS] client " << client_id << " disconnected: "
                          << info->m_info.m_szEndDebug << '\n';
            }
            SteamNetworkingSockets()->CloseConnection(info->m_hConn, 0, nullptr, false);
            break;
        }

        case k_ESteamNetworkingConnectionState_Connecting:
        {
            if (SteamNetworkingSockets()->AcceptConnection(info->m_hConn) != k_EResultOK)
            {
                SteamNetworkingSockets()->CloseConnection(info->m_hConn, 0, nullptr, false);
                std::cerr << "[GNS] AcceptConnection failed\n";
                break;
            }

            if (!SteamNetworkingSockets()->SetConnectionPollGroup(info->m_hConn, poll_group))
            {
                SteamNetworkingSockets()->CloseConnection(info->m_hConn, 0, nullptr, false);
                std::cerr << "[GNS] SetConnectionPollGroup failed\n";
                break;
            }

            uint32_t client_id;
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                client_id = next_client_id++;
                id_to_conn[client_id] = info->m_hConn;
                conn_to_id[info->m_hConn] = client_id;
            }
            std::cerr << "[GNS] client " << client_id << " accepted\n";
            break;
        }

        case k_ESteamNetworkingConnectionState_Connected:
            break;

        default:
            break;
    }
}

void GnsServerTransport::start()
{
    if (running) return;
    if (!g_init_ok)
    {
        std::cerr << "[GNS] start: init never succeeded, bailing\n";
        return;
    }

    g_server_instance = this;

    SteamNetworkingIPAddr addr;
    addr.Clear();
    addr.m_port = port;

    SteamNetworkingConfigValue_t opt;
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
               reinterpret_cast<void*>(&GnsServerTransport::staticStatusCallback));

    listen_sock = SteamNetworkingSockets()->CreateListenSocketIP(addr, 1, &opt);
    if (listen_sock == k_HSteamListenSocket_Invalid)
    {
        std::cerr << "[GNS] CreateListenSocketIP failed on port " << port << '\n';
        g_server_instance = nullptr;
        return;
    }

    poll_group = SteamNetworkingSockets()->CreatePollGroup();
    if (poll_group == k_HSteamNetPollGroup_Invalid)
    {
        std::cerr << "[GNS] CreatePollGroup failed\n";
        SteamNetworkingSockets()->CloseListenSocket(listen_sock);
        listen_sock = k_HSteamListenSocket_Invalid;
        g_server_instance = nullptr;
        return;
    }

    running = true;
}

void GnsServerTransport::stop()
{
    if (!running) return;
    running = false;

    std::vector<HSteamNetConnection> conns;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        conns.reserve(conn_to_id.size());
        for (auto& [conn, _] : conn_to_id) conns.push_back(conn);
        conn_to_id.clear();
        id_to_conn.clear();
    }
    for (auto c : conns)
        SteamNetworkingSockets()->CloseConnection(c, 0, "server shutdown", true);

    if (poll_group != k_HSteamNetPollGroup_Invalid)
    {
        SteamNetworkingSockets()->DestroyPollGroup(poll_group);
        poll_group = k_HSteamNetPollGroup_Invalid;
    }
    if (listen_sock != k_HSteamListenSocket_Invalid)
    {
        SteamNetworkingSockets()->CloseListenSocket(listen_sock);
        listen_sock = k_HSteamListenSocket_Invalid;
    }

    g_server_instance = nullptr;
}

std::vector<IncomingPacket> GnsServerTransport::poll()
{
    std::vector<IncomingPacket> out;
    if (!running) return out;

    SteamNetworkingSockets()->RunCallbacks();

    constexpr int BATCH = 32;
    ISteamNetworkingMessage* msgs[BATCH];
    for (;;)
    {
        int n = SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(poll_group, msgs, BATCH);
        if (n <= 0) break;

        for (int i = 0; i < n; ++i)
        {
            ISteamNetworkingMessage* m = msgs[i];

            uint32_t client_id = 0;
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                auto it = conn_to_id.find(m->m_conn);
                if (it != conn_to_id.end()) client_id = it->second;
            }

            if (client_id != 0 && m->m_cbSize >= 1)
            {
                const char* data = static_cast<const char*>(m->m_pData);
                PacketType type = static_cast<PacketType>(static_cast<uint8_t>(data[0]));
                std::vector<char> payload(data + 1, data + m->m_cbSize);
                out.push_back(IncomingPacket{client_id, type, std::move(payload)});
            }

            m->Release();
        }

        if (n < BATCH) break;
    }

    return out;
}

void GnsServerTransport::send(uint32_t clientId, std::vector<char> buf)
{
    HSteamNetConnection conn;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        auto it = id_to_conn.find(clientId);
        if (it == id_to_conn.end()) return;
        conn = it->second;
    }
    SteamNetworkingSockets()->SendMessageToConnection(conn, buf.data(), static_cast<uint32_t>(buf.size()),
                                                      k_nSteamNetworkingSend_Reliable | k_nSteamNetworkingSend_NoNagle, nullptr);
}

void GnsServerTransport::broadcast(const std::vector<char>& buf)
{
    std::vector<HSteamNetConnection> conns;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        conns.reserve(conn_to_id.size());
        for (auto& [c, _] : conn_to_id) conns.push_back(c);
    }
    for (auto c : conns)
        SteamNetworkingSockets()->SendMessageToConnection(c, buf.data(), static_cast<uint32_t>(buf.size()),
                                                          k_nSteamNetworkingSend_Reliable | k_nSteamNetworkingSend_NoNagle, nullptr);
}

void GnsServerTransport::broadcastExcept(uint32_t clientId, const std::vector<char>& buf)
{
    std::vector<HSteamNetConnection> conns;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        conns.reserve(conn_to_id.size());
        for (auto& [c, id] : conn_to_id)
            if (id != clientId) conns.push_back(c);
    }
    for (auto c : conns)
        SteamNetworkingSockets()->SendMessageToConnection(c, buf.data(), static_cast<uint32_t>(buf.size()),
                                                          k_nSteamNetworkingSend_Reliable | k_nSteamNetworkingSend_NoNagle, nullptr);
}

void GnsServerTransport::disconnect(uint32_t clientId)
{
    HSteamNetConnection conn;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        auto it = id_to_conn.find(clientId);
        if (it == id_to_conn.end()) return;
        conn = it->second;
        conn_to_id.erase(conn);
        id_to_conn.erase(it);
    }
    SteamNetworkingSockets()->CloseConnection(conn, 0, "kicked", true);
}

std::vector<uint32_t> GnsServerTransport::clientIds()
{
    std::vector<uint32_t> ids;
    std::lock_guard<std::mutex> lock(state_mutex);
    ids.reserve(id_to_conn.size());
    for (auto& [id, _] : id_to_conn) ids.push_back(id);
    return ids;
}

bool GnsServerTransport::isRunning() const
{
    return running;
}
