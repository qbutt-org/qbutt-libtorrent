/*
Copyright (c) 2026, qbutt contributors
Distributed under the BSD-style license in the LICENSE file.
*/

#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "libtorrent/error_code.hpp"
#include "libtorrent/info_hash.hpp"
#include "libtorrent/operations.hpp"
#include "libtorrent/socket.hpp"

namespace libtorrent {

// Opaque application identity. A context is fixed for a connection's lifetime.
// Zero identifies the ordinary session route. Credentials are never telemetry.
struct peer_route_context
{
	std::uint64_t path_id = 0;
	std::uint64_t generation = 0;
	bool operator==(peer_route_context const& rhs) const
	{ return path_id == rhs.path_id && generation == rhs.generation; }
	bool operator!=(peer_route_context const& rhs) const { return !(*this == rhs); }
};

enum class route_family { ipv4, ipv6 };

struct route_descriptor
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

	bool operator==(route_descriptor const& rhs) const
	{
		return type == rhs.type && context == rhs.context
			&& proxy_endpoint == rhs.proxy_endpoint && username == rhs.username
			&& password == rhs.password && local_endpoint == rhs.local_endpoint
			&& native_interface_index == rhs.native_interface_index;
	}
	bool operator!=(route_descriptor const& rhs) const { return !(*this == rhs); }
};

struct peer_route : route_descriptor
{
	enum class transport_t { automatic, tcp, utp };
	// Explicit uTP uses a ready registered UDP context and never falls back
	// to TCP. Automatic preserves existing session/Native behavior and uses
	// TCP for selected SOCKS routes.
	transport_t transport = transport_t::automatic;
};

struct peer_route_request
{
	info_hash_t info_hashes;
	tcp::endpoint peer;
	bool private_torrent = false;
	// Bitmask using peer_info's peer source flags.
	std::uint8_t source = 0;
	// Unknown metadata is not evidence of a public torrent.
	bool has_metadata = false;
};

// Immutable origin retained by accepted picker blocks, independently of peer
// lifetime or a later connection to the same endpoint. Contains no credentials.
struct peer_route_origin
{
	peer_route_context route;
	tcp::endpoint peer;
};

struct peer_route_observation
{
	enum class event_t { connected, activity, closed, verified };
	event_t event = event_t::activity;
	info_hash_t info_hashes;
	tcp::endpoint peer;
	peer_route_context route;
	operation_t operation = operation_t::unknown;
	error_code error;
	// Delta counters. Payload includes unverified/redundant peer data. Verified
	// credit covers accepted non-padding blocks only after piece hash success.
	std::int64_t payload_download = 0;
	std::int64_t payload_upload = 0;
	std::int64_t verified_download = 0;
	// Occupancy sampled by the peer's second_tick, not exact state-transition
	// timestamps. Demand requires outstanding requests and an unchoked peer.
	std::int64_t demand_duration_ms = 0;
	std::int64_t choked_duration_ms = 0;
	// Full connection lifetime on the closed event; never a goodput denominator.
	std::int64_t connection_duration_ms = 0;
};

// Invoked on the session network thread. Must not block or call synchronous
// session/torrent APIs. Exceptions reject the attempt without a direct fallback.
using peer_route_selector = std::function<peer_route(peer_route_request const&)>;
// The observer has the same thread/reentrancy restrictions. Exceptions are
// contained and must never change picker or socket behavior.
using peer_route_observer = std::function<void(peer_route_observation const&)>;

}
