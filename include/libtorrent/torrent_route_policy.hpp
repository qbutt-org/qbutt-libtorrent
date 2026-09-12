/*
Copyright (c) 2026, qbutt contributors
Distributed under the BSD-style license in the LICENSE file.
*/

#pragma once

#include <functional>
#include <vector>

#include "libtorrent/peer_route.hpp"

namespace libtorrent {

struct network_route
{
	route_descriptor binding;
	route_family family = route_family::ipv4;
	bool operator==(network_route const& rhs) const
	{ return binding == rhs.binding && family == rhs.family; }
};

struct torrent_route_request
{
	info_hash_t info_hashes;
	bool private_torrent = false;
	bool has_metadata = false;
};

// Complete immutable admission policy, shared by every torrent operation.
// Managed routes never inherit the session proxy, DNS or interface binding.
// The application owns edge identity and publishes a new generation whenever
// credentials, physical source or transport changes. At most 64 route/family
// descriptors are admitted. A missing pinned generation blocks private and
// metadata-less torrents, without selecting another edge.
struct torrent_route_policy
{
	enum class mode_t { session_default, managed };
	mode_t mode = mode_t::session_default;
	std::vector<network_route> routes;
	peer_route_context pinned;
	bool operator==(torrent_route_policy const& rhs) const
	{ return mode == rhs.mode && routes == rhs.routes && pinned == rhs.pinned; }
};

// Runs on the network thread before a torrent starts, when metadata arrives,
// and at the synchronous policy replacement barrier. Must not block or call
// synchronous libtorrent APIs. Exceptions and invalid new-torrent policies
// fail closed. Unknown metadata may use DHT/PEX only through its pinned route;
// private metadata disables both before any further discovery is admitted.
using torrent_route_policy_selector =
	std::function<torrent_route_policy(torrent_route_request const&)>;

}
