/*
Copyright (c) 2026, qbutt contributors
Distributed under the BSD-style license in the LICENSE file.
*/

#pragma once

#include <array>

#include "libtorrent/peer_route.hpp"

namespace libtorrent {

using trusted_inbound_token = std::array<unsigned char, 32>;

// One verified public TCP endpoint and its private loopback relay. The
// application owns the lease and replaces its generation whenever either
// endpoint changes. Libtorrent never resolves or connects to public_endpoint.
struct trusted_inbound_route
{
	peer_route_context context;
	route_family family = route_family::ipv4;
	tcp::endpoint public_endpoint;
	tcp::endpoint relay_endpoint;
	bool enable_tcp = true;

	bool operator==(trusted_inbound_route const& rhs) const
	{
		return context == rhs.context && family == rhs.family
			&& public_endpoint == rhs.public_endpoint
			&& relay_endpoint == rhs.relay_endpoint && enable_tcp == rhs.enable_tcp;
	}
	bool operator!=(trusted_inbound_route const& rhs) const { return !(*this == rhs); }
};

}
