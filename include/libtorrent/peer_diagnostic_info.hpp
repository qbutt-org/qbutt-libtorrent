/*

Copyright (c) 2026, qbutt contributors
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef TORRENT_PEER_DIAGNOSTIC_INFO_HPP_INCLUDED
#define TORRENT_PEER_DIAGNOSTIC_INFO_HPP_INCLUDED

#include "libtorrent/config.hpp"
#include "libtorrent/peer_info.hpp"
#include "libtorrent/peer_route.hpp"

namespace libtorrent {

TORRENT_VERSION_NAMESPACE_2

	// A lightweight, privacy-safe snapshot of transfer state for one connected
	// peer. Unlike peer_info, this type does not contain endpoints, client
	// identification or a piece bitfield.
	struct TORRENT_EXPORT peer_diagnostic_info
	{
		peer_route_context route{};
		peer_source_flags_t source{};
		connection_type_t connection_type{};

		// Only peer_info::interesting, peer_info::remote_choked,
		// peer_info::connecting and peer_info::handshake are populated.
		peer_flags_t flags{};

		bandwidth_state_flags_t read_state{};
		int pending_disk_bytes = 0;
		int payload_down_speed = 0;
		int down_speed = 0;
	};

TORRENT_VERSION_NAMESPACE_2_END

}

#endif
