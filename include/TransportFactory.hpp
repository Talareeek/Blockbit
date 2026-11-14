#ifndef TRANSPORT_FACTORY_HPP
#define TRANSPORT_FACTORY_HPP

#include "ServerTransport.hpp"
#include "ClientTransport.hpp"

#include <cstdint>
#include <memory>

enum class TransportKind
{
    Gns,  // GameNetworkingSockets — primary, requires BLOCKBIT_WITH_GNS=ON (default)
    Tcp,  // asio TCP — legacy, opt-in with --legacy
};

// Process-wide transport selection. Defaults to Gns; main.cpp / HeadlessMain.cpp
// flip it to Tcp when --legacy is passed. Factories below consult this value,
// so call sites in ClientGameState / HeadlessMain stay transport-agnostic.
void          setDefaultTransportKind(TransportKind kind);
TransportKind getDefaultTransportKind();

std::unique_ptr<ServerTransport> makeServerTransport(uint16_t port);

#ifndef HEADLESS_BUILD
// Client-side factory is only linked into the Blockbit (client) executable.
// Headless/dedicated server never opens an outbound connection.
std::unique_ptr<ClientTransport> makeClientTransport();
#endif

#endif // TRANSPORT_FACTORY_HPP
