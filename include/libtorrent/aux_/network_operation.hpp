/*
Copyright (c) 2026, qbutt contributors
Distributed under the BSD-style license in the LICENSE file.
*/

#pragma once

#include "libtorrent/torrent_route_policy.hpp"

namespace libtorrent { namespace aux {

// Network-thread-only cancellation identity. Each asynchronous operation owns
// its token; a torrent keeps only weak references. Revocation is irreversible,
// so removing and later readmitting a route cannot revive an old callback.
struct network_operation
{
	enum class kind_t { tracker, dht, web_seed };
	network_route route;
	kind_t kind = kind_t::tracker;
	bool aborted = false;
};

} }
