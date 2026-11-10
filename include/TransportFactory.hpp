#ifndef TRANSPORT_FACTORY_HPP
#define TRANSPORT_FACTORY_HPP

#include "ServerTransport.hpp"
#include "ClientTransport.hpp"

#include <cstdint>
#include <memory>
#include <string_view>

enum class TransportKind
{
    Tcp,  // asio TCP — original, always available
    Gns,  // GameNetworkingSockets — only when built with BLOCKBIT_WITH_GNS=ON
};

// Process-wide transport selection, set by main.cpp after parsing --transport
// (or env BB_TRANSPORT). Factories below consult this value, so call sites
// in ClientGameState / HeadlessMain stay transport-agnostic.
void          setDefaultTransportKind(TransportKind kind);
TransportKind getDefaultTransportKind();

// Returns Tcp for unrecognized strings so a typo doesn't silently fall back
// to the slower/less-safe default elsewhere; caller decides whether to warn.
bool parseTransportKind(std::string_view text, TransportKind& out);

std::unique_ptr<ServerTransport> makeServerTransport(uint16_t port);

#ifndef HEADLESS_BUILD
// Client-side factory is only linked into the Blockbit (client) executable.
// Headless/dedicated server never opens an outbound connection.
std::unique_ptr<ClientTransport> makeClientTransport();
#endif

#endif // TRANSPORT_FACTORY_HPP
