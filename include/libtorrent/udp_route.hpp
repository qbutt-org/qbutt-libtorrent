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
	enum class family_t { ipv4, ipv6 };
	family_t family = family_t::ipv4;
	bool ssl = false;
	bool enable_utp = true;
	// DHT routing and peer lookup only. These outgoing contexts do not
	// publish an announce_peer endpoint or accept unsolicited peer connections.
	bool enable_dht = false;
	bool enable_trackers = false;

	// An externally verified address, never inferred from the loopback relay.
	// DHT requires a public address of the logical family. Unspecified means
	// unknown. This descriptor does not establish public inbound capability.
	address external_address;
};

}
