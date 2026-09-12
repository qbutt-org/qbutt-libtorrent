/*
Copyright (c) 2026, qbutt contributors
Distributed under the BSD-style license in the LICENSE file.
*/

#pragma once

#include "libtorrent/socket.hpp"
#include <cstdint>

namespace libtorrent { namespace aux {

#ifdef TORRENT_WINDOWS
// WinSock uses network byte order for IPv4 and host byte order for IPv6.
// Source binding alone does not select the outgoing interface.
struct native_route_interface
{
	native_route_interface(std::uint32_t index, bool ipv6)
		: value(ipv6 ? index : htonl(index)) {}
	template <typename Protocol>
	int level(Protocol const& p) const { return p.family() == AF_INET ? IPPROTO_IP : IPPROTO_IPV6; }
	template <typename Protocol>
	int name(Protocol const& p) const { return p.family() == AF_INET ? IP_UNICAST_IF : IPV6_UNICAST_IF; }
	template <typename Protocol>
	void const* data(Protocol const&) const { return &value; }
	template <typename Protocol>
	std::size_t size(Protocol const&) const { return sizeof(value); }
	std::uint32_t value;
};
#endif

} }
