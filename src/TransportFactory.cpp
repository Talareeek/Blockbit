#include "../include/TransportFactory.hpp"

#include "../include/NetworkServerTransport.hpp"
#ifndef HEADLESS_BUILD
#include "../include/NetworkClientTransport.hpp"
#endif

#ifdef BLOCKBIT_WITH_GNS
#include "../include/GnsServerTransport.hpp"
#ifndef HEADLESS_BUILD
#include "../include/GnsClientTransport.hpp"
#endif
#endif

#include <atomic>
#include <iostream>
#include <string>

namespace
{
    std::atomic<TransportKind> g_kind{TransportKind::Tcp};
}

void setDefaultTransportKind(TransportKind kind)
{
    g_kind = kind;
}

TransportKind getDefaultTransportKind()
{
    return g_kind;
}

bool parseTransportKind(std::string_view text, TransportKind& out)
{
    if (text == "tcp" || text == "TCP") { out = TransportKind::Tcp; return true; }
    if (text == "gns" || text == "GNS") { out = TransportKind::Gns; return true; }
    return false;
}

std::unique_ptr<ServerTransport> makeServerTransport(uint16_t port)
{
    TransportKind k = g_kind;
#ifdef BLOCKBIT_WITH_GNS
    if (k == TransportKind::Gns)
        return std::make_unique<GnsServerTransport>(port);
#else
    if (k == TransportKind::Gns)
        std::cerr << "[Transport] GNS requested but build was configured without "
                     "BLOCKBIT_WITH_GNS=ON; falling back to TCP.\n";
#endif
    return std::make_unique<NetworkServerTransport>(port);
}

#ifndef HEADLESS_BUILD
std::unique_ptr<ClientTransport> makeClientTransport()
{
    TransportKind k = g_kind;
#ifdef BLOCKBIT_WITH_GNS
    if (k == TransportKind::Gns)
        return std::make_unique<GnsClientTransport>();
#else
    if (k == TransportKind::Gns)
        std::cerr << "[Transport] GNS requested but build was configured without "
                     "BLOCKBIT_WITH_GNS=ON; falling back to TCP.\n";
#endif
    return std::make_unique<NetworkClientTransport>();
}
#endif
