/*
Copyright (c) 2026, qbutt contributors
Distributed under the BSD-style license in the LICENSE file.
*/

// A bounded loopback integration fixture. No public network or payload writes.
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/bdecode.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/kademlia/node_id.hpp>
#include <libtorrent/peer_info.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_route_policy.hpp>
#include <libtorrent/udp_route.hpp>

#include <array>
#include <atomic>
#include <boost/asio/post.hpp>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace lt = libtorrent;
using namespace std::chrono_literals;

namespace {

void require(bool const value, char const* message)
{
	if (!value) throw std::runtime_error(message);
}

std::string compact(lt::address const& address, unsigned short const port)
{
	auto const bytes = address.to_v4().to_bytes();
	std::string result(bytes.begin(), bytes.end());
	result.push_back(char(port >> 8));
	result.push_back(char(port & 255));
	return result;
}

void send(lt::udp::socket& socket, lt::udp::endpoint const& destination, lt::entry const& message)
{
	std::vector<char> bytes;
	lt::bencode(std::back_inserter(bytes), message);
	socket.send_to(boost::asio::buffer(bytes), destination);
}

struct packet
{
	lt::udp::endpoint source;
	std::string transaction;
	std::string query;
	std::string id;
	char type = 0;
	bool read_only = false;
};

packet receive(lt::udp::socket& socket, std::chrono::milliseconds const timeout)
{
	auto const deadline = std::chrono::steady_clock::now() + timeout;
	std::array<char, 2048> bytes;
	while (std::chrono::steady_clock::now() < deadline)
	{
		packet result;
		lt::error_code error;
		auto const size = socket.receive_from(boost::asio::buffer(bytes), result.source, 0, error);
		if (error == boost::asio::error::would_block || error == boost::asio::error::try_again)
		{
			std::this_thread::sleep_for(5ms);
			continue;
		}
		require(!error, "DHT fixture receive failed");
		lt::bdecode_node message;
		lt::bdecode(bytes.data(), bytes.data() + size, message, error);
		require(!error, "DHT fixture received malformed bencode");
		result.transaction = message.dict_find_string_value("t").to_string();
		result.query = message.dict_find_string_value("q").to_string();
		auto const type = message.dict_find_string_value("y");
		require(type.size() == 1, "DHT fixture received invalid message type");
		result.type = type[0];
		auto const arguments = message.dict_find_dict(result.type == 'q' ? "a" : "r");
		if (arguments) result.id = arguments.dict_find_string_value("id").to_string();
		result.read_only = message.dict_find_int_value("ro") == 1;
		return result;
	}
	return {};
}

lt::entry response(packet const& request, lt::address const& observed, bool const error = false)
{
	lt::entry result;
	result["t"] = request.transaction;
	result["ip"] = compact(observed, request.source.port());
	result["y"] = error ? "e" : "r";
	if (error)
	{
		result["e"].list().emplace_back(203);
		result["e"].list().emplace_back("invalid node ID");
	}
	else
	{
		result["r"]["id"] = std::string(20, '\x42');
		result["r"]["nodes"] = std::string{};
		result["r"]["token"] = "fixture-token";
	}
	return result;
}

lt::udp_route route(std::uint64_t const generation)
{
	lt::udp_route result;
	result.route.type = lt::peer_route::type_t::native;
	result.route.context = {1, generation};
	result.route.local_endpoint = {lt::make_address("127.0.0.2"), 0};
	result.enable_dht = true;
	result.enable_utp = false;
	return result;
}

void policy(lt::session& session, lt::udp_route const& route)
{
	lt::network_route network;
	network.binding = route.route;
	network.family = route.family;
	network.public_endpoint = route.public_endpoint;
	require(!session.set_torrent_route_policy_selector([network](lt::torrent_route_request const&)
	{
		lt::torrent_route_policy result;
		result.mode = lt::torrent_route_policy::mode_t::managed;
		result.routes = {network};
		result.pinned = network.binding.context;
		return result;
	}), "Failed to install route policy");
}

packet query(lt::session& session, lt::udp::socket& server, lt::udp_route const& route)
{
	for (auto pending = receive(server, 10ms); pending.type != 0; pending = receive(server, 10ms))
		require(pending.type == 'q', "Unexpected response from an outgoing DHT route");
	require(!session.add_dht_route_node(route.route.context, route.family, server.local_endpoint(), false),
		"Failed to query the route DHT bootstrap node");
	auto request = receive(server, 2000ms);
	require(request.type == 'q' && request.id.size() == 20, "No route DHT query");
	require(request.read_only == (route.public_endpoint.port() == 0), "Incorrect DHT read-only flag");
	return request;
}

void check_tcp_retry_rejection(lt::settings_pack settings, lt::add_torrent_params add, bool const forced_utp)
{
	lt::io_context io;
	lt::udp::socket blackhole(io, {lt::address_v4::loopback(), 0});
	lt::tcp::acceptor listener(io, {lt::address_v4::loopback(), blackhole.local_endpoint().port()});
	listener.non_blocking(true);
	std::atomic<bool> closed{false}, settled{false}, revoke_error{false};
	settings.set_bool(lt::settings_pack::enable_dht, false);
	settings.set_bool(lt::settings_pack::enable_outgoing_tcp, true);
	settings.set_bool(lt::settings_pack::enable_outgoing_utp, true);
	settings.set_int(lt::settings_pack::peer_connect_timeout, 1);
	settings.set_int(lt::settings_pack::alert_mask, int(lt::alert_category::status | lt::alert_category::error));
	lt::session client(settings);
	auto path = route(forced_utp ? 10 : 11);
	path.enable_dht = false;
	path.enable_utp = true;
	require(!client.set_udp_routes({path}), "Failed to install transport fixture route");
	policy(client, path);
	bool ready = false;
	auto const ready_deadline = std::chrono::steady_clock::now() + 2s;
	while (!ready && std::chrono::steady_clock::now() < ready_deadline)
	{
		std::vector<lt::alert*> alerts;
		client.pop_alerts(&alerts);
		for (auto const* alert : alerts)
			if (auto const* udp = lt::alert_cast<lt::udp_route_alert>(alert))
				ready |= udp->route == path.route.context && udp->state == lt::udp_route_state::ready;
		std::this_thread::sleep_for(10ms);
	}
	require(ready, "Transport fixture route never became ready");
	client.set_peer_route_selector([path, forced_utp, &closed](lt::peer_route_request const&)
	{
		auto selected = path.route;
		if (!forced_utp && closed) selected.type = lt::peer_route::type_t::blocked;
		selected.transport = forced_utp ? lt::peer_route::transport_t::utp : lt::peer_route::transport_t::automatic;
		return selected;
	}, [&](lt::peer_route_observation const& event)
	{
		if (event.event != lt::peer_route_observation::event_t::closed || !event.error || closed.exchange(true)) return;
		// This precedes connect_failed's deferred retry. A second queued turn
		// acknowledges that the retry ran, without relying on a sleep.
		boost::asio::post(client.get_context(), [&]
		{
			if (!forced_utp)
			{
				revoke_error = bool(client.set_torrent_route_policy_selector([](lt::torrent_route_request const&)
				{
					lt::torrent_route_policy denied;
					denied.mode = lt::torrent_route_policy::mode_t::managed;
					return denied;
				}));
			}
			boost::asio::post(client.get_context(), [&] { settled = true; });
		});
	});
	add.save_path += forced_utp ? "/forced-utp" : "/revoked-retry";
	auto torrent = client.add_torrent(add);
	torrent.connect_peer(listener.local_endpoint());
	auto const deadline = std::chrono::steady_clock::now() + 8s;
	while (!settled && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(10ms);
	require(closed && settled && !revoke_error, "Transport rejection fixture did not exercise failure");
	require(blackhole.available() > 0, "Transport fixture never received a uTP attempt");
	lt::tcp::socket unexpected(io);
	lt::error_code error;
	listener.accept(unexpected, error);
	require(error == boost::asio::error::would_block || error == boost::asio::error::try_again,
		forced_utp ? "Forced uTP opened a TCP connection" : "Revoked generation opened a TCP connection");
}

} // anonymous namespace

int main(int argc, char* argv[]) try
{
	require(argc == 2, "Pass an isolated fixture directory");
	lt::io_context io;
	lt::udp::socket server(io, {lt::address_v4::loopback(), 0});
	lt::udp::socket wrong_port(io, {lt::address_v4::loopback(), 0});
	server.non_blocking(true);
	lt::settings_pack settings;
	settings.set_str(lt::settings_pack::listen_interfaces, "");
	settings.set_str(lt::settings_pack::dht_bootstrap_nodes, "");
	settings.set_bool(lt::settings_pack::enable_dht, true);
	settings.set_bool(lt::settings_pack::enable_lsd, false);
	settings.set_bool(lt::settings_pack::enable_upnp, false);
	settings.set_bool(lt::settings_pack::enable_natpmp, false);
	settings.set_bool(lt::settings_pack::enable_incoming_tcp, false);
	settings.set_bool(lt::settings_pack::enable_incoming_utp, false);
	settings.set_bool(lt::settings_pack::dht_enforce_node_id, false);
	settings.set_bool(lt::settings_pack::dht_restrict_routing_ips, false);
	settings.set_bool(lt::settings_pack::dht_restrict_search_ips, false);
	settings.set_int(lt::settings_pack::dht_block_ratelimit, 1000);
	lt::session session(settings);
	session.set_peer_route_selector([](lt::peer_route_request const&) { return lt::peer_route{}; });
	auto current = route(1);
	require(!session.set_udp_routes({current}), "Unknown DHT egress was rejected");
	policy(session, current);
	auto const original = query(session, server, current);
	auto const learned_address = lt::make_address("8.8.8.8");
	auto const forged_address = lt::make_address("9.9.9.9");
	auto forged = response(original, forged_address);
	forged["t"] = "";
	send(server, original.source, forged);
	send(wrong_port, original.source, response(original, forged_address));
	lt::entry unsolicited;
	unsolicited["y"] = "q";
	unsolicited["q"] = "ping";
	unsolicited["t"] = "unsolicited";
	unsolicited["ip"] = compact(forged_address, 1);
	unsolicited["a"]["id"] = std::string(20, '\x43');
	send(server, original.source, unsolicited);
	auto packet = receive(server, 150ms);
	require(packet.type != 'r' && packet.type != 'e', "Outgoing route answered an unsolicited query");
	auto const unchanged = query(session, server, current);
	require(unchanged.id == original.id, "Uncorrelated DHT packet changed route identity");
	send(server, unchanged.source, response(unchanged, learned_address, true));
	std::this_thread::sleep_for(100ms);
	auto const learned = query(session, server, current);
	require(lt::dht::verify_id(lt::dht::node_id(learned.id.data()), learned_address),
		"Correlated BEP42 error did not teach the route its external address");
	require(learned.id != original.id, "DHT identity was not updated after learning");
	send(server, learned.source, response(learned, learned_address));

	// A new generation has a fresh socket/identity. The old transaction cannot
	// teach the replacement, even if its packet is delivered to the new port.
	current = route(2);
	require(!session.set_udp_routes({current}), "Failed to replace DHT route generation");
	policy(session, current);
	auto const replacement = query(session, server, current);
	require(replacement.id != learned.id, "Replacement generation reused a DHT identity");
	require(replacement.transaction != unchanged.transaction, "Fixture transaction collision");
	send(server, replacement.source, response(unchanged, forged_address));
	std::this_thread::sleep_for(100ms);
	auto const after_stale = query(session, server, current);
	require(after_stale.id == replacement.id, "Stale generation reply changed replacement identity");
	send(server, after_stale.source, response(after_stale, learned_address));
	std::this_thread::sleep_for(100ms);

	// Exercise the torrent callback into the common peer pool, with no manual
	// peer insertion and no payload writes or outgoing peer connections.
	lt::entry metadata;
	metadata["info"]["name"] = "qbutt-dht-fixture.bin";
	metadata["info"]["length"] = 16384;
	metadata["info"]["piece length"] = 16384;
	metadata["info"]["pieces"] = std::string(20, '\x01');
	std::vector<char> encoded;
	lt::bencode(std::back_inserter(encoded), metadata);
	lt::add_torrent_params add;
	add.ti = std::make_shared<lt::torrent_info>(lt::span<char const>(encoded), lt::from_span);
	add.save_path = argv[1];
	add.file_priorities = {lt::dont_download};
	add.flags &= ~(lt::torrent_flags::paused | lt::torrent_flags::auto_managed);
	add.flags |= lt::torrent_flags::disable_pex | lt::torrent_flags::disable_lsd;
	auto const torrent = session.add_torrent(add);
	torrent.force_dht_announce();
	auto const candidate = lt::tcp::endpoint(lt::make_address("127.0.0.9"), 49001);
	bool found = false;
	int peer_queries = 0;
	auto const deadline = std::chrono::steady_clock::now() + 8s;
	while (!found && std::chrono::steady_clock::now() < deadline)
	{
		auto request = receive(server, 100ms);
		if (request.type != 0)
		{
			require(request.type == 'q' && request.read_only, "Readonly route emitted a DHT response");
			require(request.query != "announce_peer", "Outgoing route announced a false listener");
			auto reply = response(request, learned_address);
			if (request.query == "get_peers")
			{
				++peer_queries;
				reply["r"]["values"].list().emplace_back(compact(candidate.address(), candidate.port()));
			}
			send(server, request.source, reply);
		}
		std::vector<lt::peer_list_entry> peers;
		torrent.get_full_peer_list(peers);
		for (auto const& peer : peers)
			found |= peer.ip == candidate && (peer.source & static_cast<std::uint8_t>(lt::peer_info::dht));
	}
	require(found && peer_queries > 0, "DHT candidate did not reach the torrent peer pool");

	// A verified gateway descriptor retains its known identity and server role.
	current = route(3);
	current.external_address = lt::make_address("8.8.4.4");
	current.public_endpoint = {current.external_address, 42001};
	require(!session.set_udp_routes({current}), "Known public DHT descriptor was rejected");
	policy(session, current);
	auto const known = query(session, server, current);
	send(server, known.source, response(known, forged_address));
	std::this_thread::sleep_for(100ms);
	auto const protected_id = query(session, server, current);
	require(protected_id.id == known.id
		&& lt::dht::verify_id(lt::dht::node_id(protected_id.id.data()), current.external_address),
		"DHT observation replaced the verified gateway identity");
	send(server, protected_id.source, unsolicited);
	bool gateway_reply = false;
	auto const reply_deadline = std::chrono::steady_clock::now() + 2s;
	while (std::chrono::steady_clock::now() < reply_deadline && !gateway_reply)
	{
		auto reply = receive(server, 100ms);
		gateway_reply = reply.type == 'r' && reply.transaction == "unsolicited";
	}
	require(gateway_reply, "Verified gateway stopped answering DHT queries");
	require(!session.set_udp_routes({}), "Failed to retire the fixture routes");
	check_tcp_retry_rejection(settings, add, true);
	check_tcp_retry_rejection(settings, add, false);
	std::cout << "{\"passed\":true,\"unknownEgressAccepted\":true,\"correlatedBep42Learning\":true"
		<< ",\"unsolicitedAndWrongPortRejected\":true,\"staleGenerationRejected\":true"
		<< ",\"readOnly\":true,\"dhtPeerCandidates\":1,\"knownGatewayPreserved\":true"
		<< ",\"forcedUtpNoTcp\":true,\"queuedRevocationNoTcp\":true}\n";
}
catch (std::exception const& error)
{
	std::cerr << error.what() << '\n';
	return 1;
}
