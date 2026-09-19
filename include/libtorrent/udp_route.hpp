/*
Copyright (c) 2026, qbutt contributors
Distributed under the BSD-style license in the LICENSE file.
*/

#pragma once

#include "libtorrent/peer_route.hpp"

namespace libtorrent {

enum class udp_route_state { pending, ready, failed, retired };

// One UDP socket and SOCKS association per route, logical family and SSL mode.
// The physical loopback socket is independent of the remote address family.
// Register a new generation to change any descriptor or recover a failed one.
struct udp_route
{
	peer_route route;
	using family_t = route_family;
	family_t family = family_t::ipv4;
	bool ssl = false;
	bool enable_utp = true;
	// DHT routing and peer lookup. A route publishes announce_peer only when it
	// also has a verified UDP public_endpoint.
	bool enable_dht = false;
	bool enable_trackers = false;

	// The remote UDP egress address verified by the application. If unspecified,
	// outgoing-only DHT learns its node identity address from correlated replies.
	// This runtime observation never establishes a public peer listener.
	address external_address;

	// A UDP peer listener verified by the application. Its address may match
	// external_address, but it is independent of the route's generic TCP/UDP
	// tracker endpoint. An unspecified address and zero port mean outgoing-only.
	tcp::endpoint public_endpoint;
};

}
