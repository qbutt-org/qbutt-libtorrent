# qbutt-lt

qbutt-lt is the standalone public libtorrent component of qbutt. Preserve the
upstream source layout, license notices, native routing and public source history.
`upstream-lock.json` records the reviewed upstream revision. Keep the public
repository outside GitHub's fork network, with only `main` published.

Keep one session and one piece picker per torrent. Peer identity, deduplication,
source restrictions, filters and limits use the original peer endpoint within its
torrent; loopback relay endpoints must never become peer identities. Applications
select routes and own path policy; libtorrent owns peer I/O and piece scheduling.

Route identity is immutable for a connection. Replace the selector before
invalidating an old generation. Selected local SOCKS payload listeners require
loopback and mandatory authentication; rejected routes must not fall back to
Native. Native physical bindings must be explicit and preserve address family.

TCP routing does not establish Tunnels only. UDP, discovery, trackers, DNS,
webseeds, policy transitions and real egress need their own implementation and
integration evidence. Report libtorrent payload separately from verified data
and relay/wire bytes.

Preserve upstream style in existing files. Review the complete diff after changes
and remove unnecessary state, wrappers and fallback branches after a working
result. Do not write unit tests. Use generated legal fixtures and integration or
fault scenarios. Keep builds, profiles and generated artifacts outside tracked
source; use separate worktrees for concurrent subsystem implementations.

Never publish credentials, subscription URLs, private profiles or private history.
Build and validate locally. All workflows must remain `workflow_dispatch` only;
do not dispatch one without the user's explicit request. Use explicit staging
paths and verify the outgoing diff before publication.
