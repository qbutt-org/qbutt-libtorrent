/*
Copyright (c) 2026, qbutt contributors
Distributed under the BSD-style license in the LICENSE file.
*/

#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "libtorrent/info_hash.hpp"
#include "libtorrent/socket.hpp"

namespace libtorrent {

// Opaque application identity. A context is fixed for a connection's lifetime.
// Zero identifies the ordinary session route. Credentials are never telemetry.
struct peer_route_context
{
	std::uint64_t path_id = 0;
	std::uint64_t generation = 0;
};

struct peer_route
{
	enum class type_t { session_default, native, socks5, blocked };
	type_t type = type_t::session_default;
	peer_route_context context;

	// SOCKS5 routes require a numeric loopback endpoint, a nonzero context
	// and RFC 1929 credentials of 1..255 bytes. Authentication is mandatory.
	tcp::endpoint proxy_endpoint;
	std::string username;
	std::string password;

	// Optional Native TCP binding, independent of the session's proxy and
	// outgoing_interfaces. An unspecified address inherits the session binding.
	// A supplied address must match the peer's address family. On Windows,
	// native_interface_index additionally selects IP_UNICAST_IF/IPV6_UNICAST_IF
	// and requires a supplied address. Other platforms reject a nonzero index.
	tcp::endpoint local_endpoint;
	std::uint32_t native_interface_index = 0;
};

struct peer_route_request
{
	info_hash_t info_hashes;
	tcp::endpoint peer;
	bool private_torrent = false;
	// Bitmask using peer_info's peer source flags.
	std::uint8_t source = 0;
};

// Invoked on the session network thread. Must not block or call synchronous
// session/torrent APIs. Exceptions reject the attempt without a direct fallback.
using peer_route_selector = std::function<peer_route(peer_route_request const&)>;

}
