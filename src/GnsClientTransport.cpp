#include "../include/GnsClientTransport.hpp"

#include <steam/isteamnetworkingsockets.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>

namespace
{
    // One-time init matches GnsServerTransport.cpp; both TUs are safe to
    // construct independently because std::call_once is global.
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

    GnsClientTransport* g_client_instance = nullptr;
}

GnsClientTransport::GnsClientTransport()
{
    gnsEnsureInit();
}

GnsClientTransport::~GnsClientTransport()
{
    disconnect();
}

void GnsClientTransport::staticStatusCallback(SteamNetConnectionStatusChangedCallback_t* info)
{
    if (g_client_instance) g_client_instance->onConnectionStatusChanged(info);
}

void GnsClientTransport::onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info)
{
    switch (info->m_info.m_eState)
    {
        case k_ESteamNetworkingConnectionState_None:
            break;

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        {
            if (info->m_hConn == connection)
            {
                last_error = info->m_info.m_szEndDebug;
                connected = false;
                SteamNetworkingSockets()->CloseConnection(connection, 0, nullptr, false);
                connection = k_HSteamNetConnection_Invalid;
            }
            break;
        }

        case k_ESteamNetworkingConnectionState_Connecting:
            break;

        case k_ESteamNetworkingConnectionState_Connected:
            connected = true;
            break;

        default:
            break;
    }
}

bool GnsClientTransport::connect(const std::string& host, uint16_t port)
{
    if (connected) return true;
    if (!g_init_ok)
    {
        last_error = "GNS init failed";
        return false;
    }

    g_client_instance = this;

    SteamNetworkingIPAddr addr;
    addr.Clear();

    // ParseString accepts "ip" or "ip:port"; build "ip:port" since the caller
    // gives them separately. IPv6 input with embedded colons still parses
    // correctly because ParseString walks the trailing :port.
    std::string combined = host + ":" + std::to_string(port);
    if (!addr.ParseString(combined.c_str()))
    {
        last_error = "Bad address: " + combined;
        std::cerr << "[GNS] " << last_error << '\n';
        return false;
    }

    SteamNetworkingConfigValue_t opt;
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
               reinterpret_cast<void*>(&GnsClientTransport::staticStatusCallback));

    connection = SteamNetworkingSockets()->ConnectByIPAddress(addr, 1, &opt);
    if (connection == k_HSteamNetConnection_Invalid)
    {
        last_error = "ConnectByIPAddress failed";
        std::cerr << "[GNS] " << last_error << '\n';
        return false;
    }

    // Spin the callback pump until we see Connected or an error. Match the
    // old Client::connect's ~5s default budget so callers that time out on
    // the same wall-clock still behave the same.
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline)
    {
        SteamNetworkingSockets()->RunCallbacks();
        if (connected) return true;
        if (connection == k_HSteamNetConnection_Invalid) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    last_error = "Connection timed out";
    SteamNetworkingSockets()->CloseConnection(connection, 0, "timeout", false);
    connection = k_HSteamNetConnection_Invalid;
    return false;
}

void GnsClientTransport::disconnect()
{
    if (connection != k_HSteamNetConnection_Invalid)
    {
        SteamNetworkingSockets()->CloseConnection(connection, 0, "client quit", true);
        connection = k_HSteamNetConnection_Invalid;
    }
    connected = false;
    if (g_client_instance == this) g_client_instance = nullptr;
}

void GnsClientTransport::send(std::vector<char> buf)
{
    if (!connected || connection == k_HSteamNetConnection_Invalid) return;
    SteamNetworkingSockets()->SendMessageToConnection(connection, buf.data(),
                                                      static_cast<uint32_t>(buf.size()),
                                                      k_nSteamNetworkingSend_Reliable, nullptr);
}

std::vector<ReceivedPacket> GnsClientTransport::poll()
{
    std::vector<ReceivedPacket> out;
    if (connection == k_HSteamNetConnection_Invalid) return out;

    SteamNetworkingSockets()->RunCallbacks();

    constexpr int BATCH = 32;
    ISteamNetworkingMessage* msgs[BATCH];
    for (;;)
    {
        int n = SteamNetworkingSockets()->ReceiveMessagesOnConnection(connection, msgs, BATCH);
        if (n <= 0) break;

        for (int i = 0; i < n; ++i)
        {
            ISteamNetworkingMessage* m = msgs[i];
            if (m->m_cbSize >= 1)
            {
                const char* data = static_cast<const char*>(m->m_pData);
                PacketType type = static_cast<PacketType>(static_cast<uint8_t>(data[0]));
                std::vector<char> payload(data + 1, data + m->m_cbSize);
                out.push_back(ReceivedPacket{type, std::move(payload)});
            }
            m->Release();
        }

        if (n < BATCH) break;
    }

    return out;
}

bool GnsClientTransport::isConnected() const
{
    return connected;
}

const std::string& GnsClientTransport::getLastError() const
{
    return last_error;
}
