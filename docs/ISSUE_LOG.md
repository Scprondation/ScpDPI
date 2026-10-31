# Discord web and RTC troubleshooting log

This is the running engineering report for the reported Discord browser and voice problems. Entries separate user-reported symptoms from observations made on this machine. Append new investigation results with a date and mark them as reported, observed, or reproduced.

## 2026-10-31 — Investigation and diagnostic changes

### Reported symptoms

- User reports that Discord opens in its desktop application but does not open in a browser.
- User reports that Discord voice/RTC remains connecting. They report that it works with a VPN.
- These symptoms were not reproduced in a browser or live voice session during this investigation.

### Machine and build observations

- Repository was on `main` at `fa35336` (`Improve Discord and QUIC DPI handling`) before this investigation, with no local changes.
- The installed service executable under `C:\Program Files\ScpDPI\versions\build-134379361477000793-536576` and the repository release build had the same SHA-256: `D2E81A3A7C7A6BD42EB10C5E650F56908CDF3E35BC894768E270AA07A72C791C`. This confirms that the installed binary contains the previous Discord/QUIC changes.
- Scheduled task `ScpDPI DPI Background` exists and is configured to launch the installed helper with `--serve` at user logon. At inspection it showed `Ready`; its last run was reported as successful, but no ScpDPI process was present at that instant. A successful helper exit does not prove that its child service is still alive.
- `%LOCALAPPDATA%\ScpDPI\config.json` has file logging enabled at `info` level and DPI enabled.
- `%LOCALAPPDATA%\ScpDPI\scpdpi.log` was zero bytes and had not changed since 14:37. The logger did not flush normal records while the long-running service stayed open, so this file could not serve as a useful live diagnostic. The underlying process absence also means current packet counters were unavailable. The empty log is not evidence that no traffic was seen.

### GoodbyeDPI review

The official GoodbyeDPI README describes passive reset handling and several active TCP/HTTP approaches, including stream segmentation, HTTP header transformations, and forged packets with altered delivery properties. Its published license is Apache-2.0. ScpDPI already had listed-host TLS segmentation, listed QUIC fallback, and limited Discord/STUN discovery handling, but it did not suppress a matching inbound passive-DPI reset. The browser failure could therefore persist even when the outbound ClientHello is split. The license is not a legal guarantee for a separate product; ScpDPI uses no copied GoodbyeDPI source and this report is not a legal opinion.

The RTC symptom has a different transport requirement. ScpDPI does not proxy or tunnel voice media; its UDP discovery decoy cannot carry a voice session. If the direct network blocks the UDP path Discord needs, the VPN working is consistent with that limitation. Packet counters can show whether Discord/STUN discovery requests reach ScpDPI, but they cannot make a blocked UDP route usable by themselves.

### Changes made in this investigation

- File logging now flushes records at `info` level and rotates at 5 MiB with three backups, so startup, DPI errors, and periodic diagnostics become visible while the background process is running without unbounded log growth.
- The service now writes one aggregate diagnostic snapshot per minute: interface/route/VPN counts and DPI packet, TLS, fallback, reset, and voice-discovery counters. It does not record packet payloads, addresses, or hostnames.
- For an active TLS flow whose SNI matched ScpDPI's configured lists, the packet engine now suppresses an inbound IPv4 TCP reset only when its IP ID is 0 or 1. Other resets and unrelated traffic pass through. The console exposes the number filtered.
- Added the GoodbyeDPI review and implementation provenance to `docs/THIRD_PARTY.md`. No GoodbyeDPI source, presets, packet templates, or host lists were copied.

### Verification state

- The `scpdpi_service` Release target compiled successfully with MSVC 14.52 in the Visual Studio 2026 x64 developer environment. Output: `out/build/windows-console-release-vpnfix/src/service/scpdpi_service.exe` (539,136 bytes).
- An initial build attempt from a plain PowerShell session failed before compiling because the MSVC standard-library include environment was absent; rerunning from `vcvars64.bat` succeeded.
- No tests or live Discord browser/RTC test were run. No packet payload capture was taken.
- After the new build is installed and the service has run, capture `dpi status` after trying Discord web once and RTC once, along with `%LOCALAPPDATA%\ScpDPI\scpdpi.log`. Avoid including unrelated personal traffic details when sharing logs.

## 2026-10-31 19:16–19:17 — User supplied runtime report (VPN off)

The user supplied two service snapshots and confirmed the VPN was off:

- Startup: 4 interfaces, 32 routes, 2 VPN candidates, 2 tunnel interfaces; DPI enabled and running.
- About one minute later: 3 interfaces, 23 routes, 1 VPN candidate, 1 tunnel interface; 3,755 packets inspected, 1,751 outbound TLS candidate packets, 84 ClientHello parses, 81 unlisted/invalid attempts, 0 timeouts, 19 listed-host matches, 75 split packets, 0 split/send failures, 0 passive resets filtered, 0 QUIC fallbacks, 6 voice discovery requests, 4 decoys sent, 0 decoy failures.
- The previous interface report from this machine included a Teredo tunneling adapter. The current `tunnel_interfaces=1` with VPN disabled may be Teredo or another virtual adapter; the aggregate log does not identify it.

Verbatim log excerpt supplied by the user:

```text
[2026-10-31 19:16:41.149] [scpdpi] [info] ScpDPI service initialized with profile default
[2026-10-31 19:16:41.785] [scpdpi] [info] DPI packet processor started
[2026-10-31 19:16:41.788] [scpdpi] [info] Diagnostic snapshot: interfaces=4 routes=32 vpn_candidates=2 dpi_enabled=true dpi_running=true packets=0 tcp_tls_candidates=0 tls_parsed=0 tls_unlisted=0 tls_timeouts=0 listed_tls_matches=0 split_packets=0 split_failures=0 send_failures=0 passive_resets_dropped=0 quic_fallbacks=0 voice_discovery_requests=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=2 strategy_changes=0
[2026-10-31 19:17:41.988] [scpdpi] [info] Diagnostic snapshot: interfaces=3 routes=23 vpn_candidates=1 dpi_enabled=true dpi_running=true dpi_error= packets=3755 tcp_tls_candidates=1751 tls_parsed=84 tls_unlisted=81 tls_timeouts=0 listed_tls_matches=19 split_packets=75 split_failures=0 send_failures=0 passive_resets_dropped=0 quic_fallbacks=0 voice_discovery_requests=6 udp_decoys_sent=4 udp_decoys_failed=0 tunnel_interfaces=1 strategy_changes=0
```

Interpretation: the packet processor was active and did handle listed TLS flows; those aggregate counts do not establish that the 19 matches were Discord. The zero reset count means the new passive-reset filter did not observe its narrow signature. Four sent UDP decoys show successful local reinjection only; they do not confirm end-to-end voice media. Discord's Russian help article lists “RTC Connecting” among client-side connection errors and says VPNs must support UDP.

Code inspection found that QUIC handling was disabled globally whenever any tunnel adapter existed (`tunnel || has_tunnels()`), even for packets routed directly over Ethernet. With VPN disabled but a tunnel adapter present, this can explain why listed QUIC never fell back to TCP. The global gate is removed in favor of the egress interface of each packet. Teredo is excluded from VPN candidate reporting. New aggregate counters distinguish Discord TLS/QUIC from other listed sites and direct UDP candidates from tunnel-routed candidates, without recording hostnames or packet contents. Generic voice-port candidates are labeled separately from parsed Discord IP discovery/STUN requests.

### Follow-up verification

- The `scpdpi_service` Release target compiled successfully with MSVC 14.52 after the per-packet QUIC route change, Teredo classification correction, and diagnostic counters. Binary: `out/build/windows-console-release-vpnfix/src/service/scpdpi_service.exe` (545,280 bytes).
- `git diff --check` reported no whitespace errors. No tests or live Discord browser/RTC test were run.
- The user's browser/RTC result still needs confirmation after installing that build. If RTC continues to fail directly, ScpDPI cannot substitute for a UDP-capable route or VPN.

## 2026-10-31 19:25–19:26 — Follow-up runtime report and UDP filter gap

The user supplied updated service logs and `C:\Users\scprondation\AppData\Local\ScpDPI\config.json`. The relevant config is `profile=default`, DPI engine enabled, 3-second scan interval, file logging enabled at `info`, and unknown traffic set to bypass. No setting in this file disables DPI or voice discovery.

Verbatim new snapshots:

```text
[2026-10-31 19:25:22.951] [scpdpi] [info] ScpDPI service initialized with profile default
[2026-10-31 19:25:23.580] [scpdpi] [info] DPI packet processor started
[2026-10-31 19:25:23.581] [scpdpi] [info] Diagnostic snapshot: interfaces=2 routes=17 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=0 tcp_tls_candidates=0 tcp_tls_tunnel_candidates=0 tls_parsed=0 tls_unlisted=0 tls_timeouts=0 listed_tls_matches=0 discord_tls_matches=0 split_packets=0 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=0 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=0 strategy_changes=0
[2026-10-31 19:26:23.777] [scpdpi] [info] Diagnostic snapshot: interfaces=3 routes=23 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=24319 tcp_tls_candidates=9548 tcp_tls_tunnel_candidates=0 tls_parsed=101 tls_unlisted=62 tls_timeouts=14 listed_tls_matches=48 discord_tls_matches=12 split_packets=191 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=483 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=1 strategy_changes=0
```

The second snapshot confirms ScpDPI recognized 12 Discord TLS ClientHellos and resegmented listed traffic without split or reinjection failures. There were 483 outbound UDP/443 candidates but no listed QUIC Initial fallback. There were no voice discovery packets on the UDP ports the current build watched. That zero is not proof that Discord sent no discovery packet: the UDP filter only included a fixed port list.

Discord's current voice protocol documentation says the server's `Ready` message supplies the UDP address and port and the client opens UDP to that assigned endpoint. The observed fixed-port-only capture could therefore miss Discord's discovery request. The follow-up change selects outbound UDP packets whose payload length is exactly 74 bytes, regardless of destination port, then validates the protocol signature before counting or creating any discovery decoy. Other UDP packets remain unmodified and are immediately reinjected. A new counter records valid Discord discovery packets outside the previous candidate-port list without logging their port or contents.

### Follow-up verification

- The dynamic-port discovery filter, packet validation, and additional counter compiled successfully with MSVC 14.52 for `scpdpi_service` Release. Binary: `out/build/windows-console-release-vpnfix/src/service/scpdpi_service.exe` (545,792 bytes).
- `git diff --check` reported no whitespace errors. No tests or live Discord RTC session were run.
- These snapshots do not establish whether RTC was attempted during the measured minute. If a new run still reports zero Discord IP discovery after attempting to join a voice channel, the next check is Discord's browser developer console and an external route/firewall check; the packet log alone cannot distinguish “Discord sent no UDP” from “UDP never reached this capture layer.”

## 2026-10-31 19:31–19:33 — YouTube regression report

The user reports that YouTube stopped responding and supplied the remainder of the log. The service restarted at 19:31:28 and the new `discord_ip_discovery_other_ports` field is present, confirming that the dynamic-port build was running. At 19:32, stats include 16 listed TLS matches (14 Discord), 63 split packets, 33 UDP/443 candidates, and zero QUIC fallback. At 19:33, stats include 36 listed TLS matches (24 Discord), 143 split packets, 209 UDP/443 candidates, and zero QUIC fallback. Both snapshots show zero split/reinjection failures and zero voice discovery requests. The user had VPN disabled in the previous confirmation; this excerpt has one recognized tunnel interface at both snapshots.

Verbatim continuation from the supplied log:

```text
[2026-10-31 19:27:24.016] [scpdpi] [info] Diagnostic snapshot: interfaces=3 routes=23 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=27632 tcp_tls_candidates=11479 tcp_tls_tunnel_candidates=0 tls_parsed=142 tls_unlisted=110 tls_timeouts=14 listed_tls_matches=55 discord_tls_matches=17 split_packets=219 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=496 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=1 strategy_changes=0
[2026-10-31 19:28:24.311] [scpdpi] [info] Diagnostic snapshot: interfaces=4 routes=32 vpn_candidates=1 dpi_enabled=true dpi_running=true dpi_error= packets=33527 tcp_tls_candidates=14949 tcp_tls_tunnel_candidates=767 tls_parsed=262 tls_unlisted=242 tls_timeouts=14 listed_tls_matches=57 discord_tls_matches=19 split_packets=227 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=551 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=2 strategy_changes=0
[2026-10-31 19:29:24.577] [scpdpi] [info] Diagnostic snapshot: interfaces=4 routes=32 vpn_candidates=1 dpi_enabled=true dpi_running=true dpi_error= packets=39960 tcp_tls_candidates=17510 tcp_tls_tunnel_candidates=2079 tls_parsed=364 tls_unlisted=355 tls_timeouts=14 listed_tls_matches=57 discord_tls_matches=19 split_packets=227 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=1396 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=2 strategy_changes=0
[2026-10-31 19:30:24.858] [scpdpi] [info] Diagnostic snapshot: interfaces=4 routes=32 vpn_candidates=1 dpi_enabled=true dpi_running=true dpi_error= packets=47122 tcp_tls_candidates=20004 tcp_tls_tunnel_candidates=3502 tls_parsed=483 tls_unlisted=487 tls_timeouts=14 listed_tls_matches=57 discord_tls_matches=19 split_packets=227 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=3123 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=2 strategy_changes=0
[2026-10-31 19:31:25.139] [scpdpi] [info] Diagnostic snapshot: interfaces=4 routes=32 vpn_candidates=1 dpi_enabled=true dpi_running=true dpi_error= packets=53337 tcp_tls_candidates=22541 tcp_tls_tunnel_candidates=4606 tls_parsed=591 tls_unlisted=609 tls_timeouts=14 listed_tls_matches=57 discord_tls_matches=19 split_packets=227 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=4216 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=2 strategy_changes=0
[2026-10-31 19:31:28.396] [scpdpi] [info] ScpDPI service initialized with profile default
[2026-10-31 19:31:29.015] [scpdpi] [info] DPI packet processor started
[2026-10-31 19:31:29.017] [scpdpi] [info] Diagnostic snapshot: interfaces=2 routes=17 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=0 tcp_tls_candidates=0 tcp_tls_tunnel_candidates=0 tls_parsed=0 tls_unlisted=0 tls_timeouts=0 listed_tls_matches=0 discord_tls_matches=0 split_packets=0 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=0 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=0 strategy_changes=0
[2026-10-31 19:32:29.227] [scpdpi] [info] Diagnostic snapshot: interfaces=3 routes=23 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=1929 tcp_tls_candidates=1033 tcp_tls_tunnel_candidates=0 tls_parsed=58 tls_unlisted=45 tls_timeouts=0 listed_tls_matches=16 discord_tls_matches=14 split_packets=63 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=33 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 voice_udp_port_candidates=18 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=1 strategy_changes=0
[2026-10-31 19:33:29.464] [scpdpi] [info] Diagnostic snapshot: interfaces=3 routes=23 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=4634 tcp_tls_candidates=2454 tcp_tls_tunnel_candidates=0 tls_parsed=117 tls_unlisted=97 tls_timeouts=10 listed_tls_matches=36 discord_tls_matches=24 split_packets=143 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=209 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 voice_udp_port_candidates=42 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=1 strategy_changes=0
```

The current logs cannot attribute the non-Discord TLS matches to YouTube, nor show that a specific YouTube request failed. UDP/443 traffic was present but no listed QUIC Initial was recognized. Separate YouTube TLS and QUIC counters are added for the next measurement. To reduce collateral capture after the regression report, the 74-byte UDP filter is narrowed to the Discord discovery header (type 1, declared length 70, nonzero SSRC); the full payload signature is still validated before any decoy. Other matching UDP datagrams are forwarded unchanged.

### Follow-up verification

- The narrower Discord UDP signature filter and per-site YouTube TLS/QUIC counters compiled successfully with MSVC 14.52 for `scpdpi_service` Release. Binary: `out/build/windows-console-release-vpnfix/src/service/scpdpi_service.exe` (547,328 bytes).
- `git diff --check` reported no whitespace errors. No tests or live YouTube/RTC comparison were run.
- A `dpi off` then YouTube check is a reversible A/B diagnostic before replacing the currently installed build.

## 2026-10-31 19:39–19:41 — Follow-up run with YouTube counters

The user supplied the next three snapshots after the previous diagnostic build. The service restarted at 19:39:34. At 19:40 and 19:41, ScpDPI recorded 9 then 11 YouTube TLS ClientHello matches, and 127 then 137 resegmented packets overall. Split and packet reinjection failures remained zero. UDP/443 candidates rose from 202 to 220, but no YouTube QUIC fallback was recognized. Discord discovery registered 4 direct requests and 2 decoys sent, with no decoy failures. One tunnel interface was reported in both non-startup snapshots; the user previously stated that the VPN was off, so this aggregate count does not identify that interface.

Important counter distinction from code inspection: `tls_timeouts` is `tls_hellos_timed_out`, incremented when ScpDPI holds an incomplete outbound TLS ClientHello for its pending-flow timeout. It does not measure a missing YouTube server response. Separately, the packet engine waits 8 seconds for a TLS response after sending a listed/resegmented ClientHello; expiry advances the hostname's strategy and increments only the global `strategy_changes`. The existing counters therefore could not say whether a YouTube connection received TLS data, an alert, a reset, or no response.

The next diagnostic change adds separate per-site outcome counters for Discord and YouTube: inbound TLS response observed, TLS alert, TCP reset, and response timeout. “Response observed” means the engine saw an inbound TLS record beginning with handshake or application-data content; it does not assert that the browser completed loading the site. These counters contain no hostnames or packet contents and are included in `dpi status`, IPC status, and periodic service snapshots.

Verbatim new snapshots supplied by the user:

```text
[2026-10-31 19:39:34.666] [scpdpi] [info] ScpDPI service initialized with profile default
[2026-10-31 19:39:35.289] [scpdpi] [info] DPI packet processor started
[2026-10-31 19:39:35.290] [scpdpi] [info] Diagnostic snapshot: interfaces=2 routes=17 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=0 tcp_tls_candidates=0 tcp_tls_tunnel_candidates=0 tls_parsed=0 tls_unlisted=0 tls_timeouts=0 listed_tls_matches=0 discord_tls_matches=0 youtube_tls_matches=0 split_packets=0 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=0 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 youtube_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=0 strategy_changes=0
[2026-10-31 19:40:35.492] [scpdpi] [info] Diagnostic snapshot: interfaces=3 routes=23 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=4382 tcp_tls_candidates=2231 tcp_tls_tunnel_candidates=0 tls_parsed=79 tls_unlisted=52 tls_timeouts=6 listed_tls_matches=32 discord_tls_matches=13 youtube_tls_matches=9 split_packets=127 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=202 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 youtube_quic_fallbacks=0 voice_udp_port_candidates=35 voice_udp_tunnel_candidates=0 voice_discovery_requests=4 voice_discovery_direct=4 voice_discovery_tunnel=0 discord_ip_discovery=4 discord_ip_discovery_other_ports=0 udp_decoys_sent=2 udp_decoys_failed=0 tunnel_interfaces=1 strategy_changes=1
[2026-10-31 19:41:35.680] [scpdpi] [info] Diagnostic snapshot: interfaces=3 routes=23 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=8834 tcp_tls_candidates=5050 tcp_tls_tunnel_candidates=0 tls_parsed=126 tls_unlisted=117 tls_timeouts=8 listed_tls_matches=34 discord_tls_matches=13 youtube_tls_matches=11 split_packets=137 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=220 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 youtube_quic_fallbacks=0 voice_udp_port_candidates=35 voice_udp_tunnel_candidates=0 voice_discovery_requests=4 voice_discovery_direct=4 voice_discovery_tunnel=0 discord_ip_discovery=4 discord_ip_discovery_other_ports=0 udp_decoys_sent=2 udp_decoys_failed=0 tunnel_interfaces=1 strategy_changes=3
```

### Diagnostic build

- MSVC 14.52 Release build of `scpdpi_service` succeeded at `out/build/windows-console-release-vpnfix/src/service/scpdpi_service.exe`.
- `git diff --check` reported no whitespace errors. No tests and no live browser/network verification were run.
- The new per-site outcome counters only collect evidence after this diagnostic build is installed and YouTube/Discord traffic is tried. They do not yet establish the cause of the YouTube regression or prove that a future change restores it.

## 2026-10-31 19:47 — Browser voice works; desktop app voice does not

The user reports that Discord voice now works in the browser, while voice in the desktop application still does not. The supplied file contains the 19:47:38 service startup snapshot from the `c6c1391` build, with zero traffic and zero voice/TLS counters. It ends before a later periodic snapshot, so it contains no attempted desktop-app session and cannot identify what differs between the two clients.

The packet engine uses a system-wide WinDivert network filter; the current stats do not identify the originating process. It can count matching TLS and voice-discovery packets, but it does not tunnel arbitrary UDP voice media. Thus browser success shows that at least one client/network path can connect, while the desktop-app result could still involve an app-specific firewall/VPN rule or audio-device issue; these are possibilities, not established causes. Discord's support article recommends checking the firewall/antivirus allowlist and confirms VPN voice requires UDP support.

Next evidence needed: with the `c6c1391` build running, attempt one desktop-app voice connection for at least one minute and capture the following diagnostic snapshot. The key counters are `voice_discovery_requests`, `discord_ip_discovery`, `udp_decoys_sent`, `voice_udp_tunnel_candidate_packets`, and the Discord TLS outcomes. Also clarify whether the app hangs at RTC/Connecting or joins with no audio; those symptoms point to different paths.

Verbatim new startup line supplied by the user:

```text
[2026-10-31 19:47:38.238] [scpdpi] [info] Diagnostic snapshot: interfaces=3 routes=23 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=0 tcp_tls_candidates=0 tcp_tls_tunnel_candidates=0 tls_parsed=0 tls_unlisted=0 tls_timeouts=0 listed_tls_matches=0 discord_tls_matches=0 youtube_tls_matches=0 discord_tls_rx=0 discord_tls_alerts=0 discord_tls_resets=0 discord_tls_response_timeouts=0 youtube_tls_rx=0 youtube_tls_alerts=0 youtube_tls_resets=0 youtube_tls_response_timeouts=0 split_packets=0 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=0 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 youtube_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=1 strategy_changes=0
```

The user clarified that the desktop client starts connecting and then shows a red error. Based on Discord's documented voice sequence, added a narrow inbound capture for the 74-byte Discord IP-discovery response (type 2, declared length 70, nonzero SSRC). The engine correlates it to an earlier request by reversed IP/UDP tuple and SSRC, then increments `discord_ip_discovery_responses`; response packets are reinjected unchanged. This distinguishes “request observed, no matching reply observed” from a completed IP-discovery exchange without recording IPs, ports, or payloads. It still does not tell whether the subsequent encrypted UDP media path works.

### Follow-up verification

- MSVC 14.52 Release build of `scpdpi_service` succeeded at `out/build/windows-console-release-vpnfix/src/service/scpdpi_service.exe`.
- `git diff --check` reported no whitespace errors. No tests or live Discord app session were run.
- The user needs to install/run this build and try the desktop app by itself for at least one minute; the 19:47 log was only the startup snapshot from the previous diagnostic build.

## 2026-10-31 19:53–19:55 — Desktop voice connection still fails

The user reports the desktop client starts connecting and then shows a red error. In the supplied run, Discord TLS receives were observed for every listed Discord ClientHello counted (12/12 at 19:54 and 29/29 at 19:55), with no Discord TLS alert, reset, or response timeout. The packet engine also observed 4 then 7 direct voice discovery requests and sent the same number of decoys without send failures. `discord_ip_discovery=0` means none of these was the fixed Discord IP-discovery message; given the current parser, the observed requests are STUN Binding Requests. `voice_udp_tunnel_candidate_packets=0` and `udp443_tunnel_candidates=0` show the counted candidates were not routed through an identified tunnel. These counters do not show whether the STUN servers returned a response.

The user-facing log added in commit `fb6c234` counted responses only for Discord's fixed 74-byte IP-discovery exchange. Since this run uses STUN instead, the diagnostics have been extended to capture only inbound STUN Binding Success/Error response headers (UDP payload at least 20 bytes, response type and magic cookie), then validate the length/cookie and correlate the 96-bit transaction ID and reversed UDP tuple to an observed request. Response packets remain unchanged. New counters are `stun_binding_requests`, `stun_binding_success_responses`, and `stun_binding_error_responses`.

Verbatim latest snapshots supplied by the user:

```text
[2026-10-31 19:53:22.279] [scpdpi] [info] ScpDPI service initialized with profile default
[2026-10-31 19:53:22.903] [scpdpi] [info] DPI packet processor started
[2026-10-31 19:53:22.905] [scpdpi] [info] Diagnostic snapshot: interfaces=2 routes=17 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=0 tcp_tls_candidates=0 tcp_tls_tunnel_candidates=0 tls_parsed=0 tls_unlisted=0 tls_timeouts=0 listed_tls_matches=0 discord_tls_matches=0 youtube_tls_matches=0 discord_tls_rx=0 discord_tls_alerts=0 discord_tls_resets=0 discord_tls_response_timeouts=0 youtube_tls_rx=0 youtube_tls_alerts=0 youtube_tls_resets=0 youtube_tls_response_timeouts=0 split_packets=0 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=0 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 youtube_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_responses=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=0 strategy_changes=0
[2026-10-31 19:54:23.114] [scpdpi] [info] Diagnostic snapshot: interfaces=3 routes=23 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=10391 tcp_tls_candidates=4357 tcp_tls_tunnel_candidates=0 tls_parsed=70 tls_unlisted=55 tls_timeouts=4 listed_tls_matches=25 discord_tls_matches=12 youtube_tls_matches=6 discord_tls_rx=12 discord_tls_alerts=0 discord_tls_resets=0 discord_tls_response_timeouts=0 youtube_tls_rx=5 youtube_tls_alerts=0 youtube_tls_resets=0 youtube_tls_response_timeouts=1 split_packets=100 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=244 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 youtube_quic_fallbacks=0 voice_udp_port_candidates=8 voice_udp_tunnel_candidates=0 voice_discovery_requests=4 voice_discovery_direct=4 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_responses=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=4 udp_decoys_failed=0 tunnel_interfaces=1 strategy_changes=1
[2026-10-31 19:55:23.302] [scpdpi] [info] Diagnostic snapshot: interfaces=3 routes=23 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=16755 tcp_tls_candidates=7570 tcp_tls_tunnel_candidates=0 tls_parsed=113 tls_unlisted=82 tls_timeouts=4 listed_tls_matches=51 discord_tls_matches=29 youtube_tls_matches=6 discord_tls_rx=29 discord_tls_alerts=0 discord_tls_resets=0 discord_tls_response_timeouts=0 youtube_tls_rx=5 youtube_tls_alerts=0 youtube_tls_resets=0 youtube_tls_response_timeouts=1 split_packets=203 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=292 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 youtube_quic_fallbacks=0 voice_udp_port_candidates=25 voice_udp_tunnel_candidates=0 voice_discovery_requests=7 voice_discovery_direct=7 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_responses=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=7 udp_decoys_failed=0 tunnel_interfaces=1 strategy_changes=1
```

### Follow-up verification

- MSVC 14.52 Release build of `scpdpi_service` succeeded at `out/build/windows-console-release-vpnfix/src/service/scpdpi_service.exe` with the STUN response counters.
- `git diff --check` reported no whitespace errors. No tests or live Discord app session were run.
- The previous log is consistent with a UDP/STUN-stage failure but cannot prove whether a response was blocked or whether a later voice stage failed. Test the desktop app alone with this build for at least one minute and inspect the three new STUN counters.

## 2026-10-31 19:58 — STUN telemetry build startup only

The user supplied the startup snapshot from the build containing STUN counters. All traffic counters are zero at 19:58:03, immediately after the DPI packet processor starts. There is no later periodic snapshot in this excerpt, so it does not yet contain a Discord voice attempt or any STUN request/response result. The user should attempt the desktop voice connection and provide the next minute snapshot.

Verbatim line supplied by the user:

```text
[2026-10-31 19:58:03.791] [scpdpi] [info] Diagnostic snapshot: interfaces=2 routes=17 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=0 tcp_tls_candidates=0 tcp_tls_tunnel_candidates=0 tls_parsed=0 tls_unlisted=0 tls_timeouts=0 listed_tls_matches=0 discord_tls_matches=0 youtube_tls_matches=0 discord_tls_rx=0 discord_tls_alerts=0 discord_tls_resets=0 discord_tls_response_timeouts=0 youtube_tls_rx=0 youtube_tls_alerts=0 youtube_tls_resets=0 youtube_tls_response_timeouts=0 split_packets=0 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=0 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 youtube_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_responses=0 stun_binding_requests=0 stun_binding_success_responses=0 stun_binding_error_responses=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=0 strategy_changes=0
```

## 2026-10-31 20:00 — Active snapshot, no recognized voice discovery

The user supplied a startup snapshot at 19:59 and a traffic snapshot at 20:00. The latter reports 14 Discord TLS matches and 13 inbound TLS records; it also reports one Discord TLS reset, one Discord TLS response timeout, one filtered passive reset, and one global strategy change. These may describe the same attempt, but the aggregate counters cannot prove that. There were 18 outbound UDP packets to the engine's watched voice-candidate ports, but zero recognized Discord IP-discovery or STUN Binding requests, and therefore zero STUN responses to correlate. If this minute was the desktop-app test, it did not reach a recognized voice discovery exchange. It is possible that the application failed earlier at its voice WebSocket/TLS stage, but the general Discord TLS counter cannot separate `discord.media` voice-server connections from other Discord TLS hosts.

Added a privacy-preserving `discord.media` category for TLS matches and outcomes (response, alert, reset, response timeout). This domain is used for Discord voice server endpoints in the public protocol guide. The new counters should show whether a reset/timeout is on the voice-server connection specifically. No hostname or packet contents are logged.

Verbatim snapshots supplied by the user:

```text
[2026-10-31 19:59:42.562] [scpdpi] [info] ScpDPI service initialized with profile default
[2026-10-31 19:59:43.192] [scpdpi] [info] DPI packet processor started
[2026-10-31 19:59:43.195] [scpdpi] [info] Diagnostic snapshot: interfaces=4 routes=32 vpn_candidates=1 dpi_enabled=true dpi_running=true dpi_error= packets=0 tcp_tls_candidates=0 tcp_tls_tunnel_candidates=0 tls_parsed=0 tls_unlisted=0 tls_timeouts=0 listed_tls_matches=0 discord_tls_matches=0 youtube_tls_matches=0 discord_tls_rx=0 discord_tls_alerts=0 discord_tls_resets=0 discord_tls_response_timeouts=0 youtube_tls_rx=0 youtube_tls_alerts=0 youtube_tls_resets=0 youtube_tls_response_timeouts=0 split_packets=0 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=0 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 youtube_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_responses=0 stun_binding_requests=0 stun_binding_success_responses=0 stun_binding_error_responses=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=2 strategy_changes=0
[2026-10-31 20:00:43.354] [scpdpi] [info] Diagnostic snapshot: interfaces=3 routes=23 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=3030 tcp_tls_candidates=1707 tcp_tls_tunnel_candidates=202 tls_parsed=58 tls_unlisted=45 tls_timeouts=0 listed_tls_matches=16 discord_tls_matches=14 youtube_tls_matches=0 discord_tls_rx=13 discord_tls_alerts=0 discord_tls_resets=1 discord_tls_response_timeouts=1 youtube_tls_rx=0 youtube_tls_alerts=0 youtube_tls_resets=0 youtube_tls_response_timeouts=0 split_packets=62 split_failures=0 send_failures=0 passive_resets_dropped=1 udp443_candidates=17 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 youtube_quic_fallbacks=0 voice_udp_port_candidates=18 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_responses=0 stun_binding_requests=0 stun_binding_success_responses=0 stun_binding_error_responses=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=1 strategy_changes=1
```

### Follow-up verification

- MSVC 14.52 Release build of `scpdpi_service` succeeded with `discord.media` TLS outcome counters at `out/build/windows-console-release-vpnfix/src/service/scpdpi_service.exe`.
- `git diff --check` reported no whitespace errors. No tests or live Discord app session were run.

## References

- [GoodbyeDPI official repository and behavior description](https://github.com/ValdikSS/GoodbyeDPI)
- [GoodbyeDPI published license](https://github.com/ValdikSS/GoodbyeDPI/blob/master/LICENSE)
- [Discord: Ошибки голосового соединения](https://support.discord.com/hc/ru/articles/115001310031-%D0%9E%D1%88%D0%B8%D0%B1%D0%BA%D0%B8-%D0%B3%D0%BE%D0%BB%D0%BE%D1%81%D0%BE%D0%B2%D0%BE%D0%B3%D0%BE-%D1%81%D0%BE%D0%B5%D0%B4%D0%B8%D0%BD%D0%B5%D0%BD%D0%B8%D1%8F)
- [Discord voice connection protocol](https://docs.discord.com/developers/topics/voice-connections)

## 2026-10-31 20:04–20:05 — Discord voice TLS replies observed; no discovery packet recognized

The user supplied three more service startups at 20:04:01, 20:04:09, and 20:04:17. Each restart reset the diagnostic counters. The 20:05:18 snapshot is the only traffic snapshot after those restarts, covering roughly one minute. It reports five `discord.media` TLS matches and five inbound TLS records for that category, with no counted TLS alerts, resets, or response timeouts. This indicates inbound TLS data was seen on those connections; it does not prove the Discord app completed its voice WebSocket exchange or joined the UDP media stage.

During the same interval, `voice_udp_port_candidates=22`, but `discord_ip_discovery=0`, `stun_binding_requests=0`, and `udp_decoys_sent=0`. The service therefore saw some UDP packets to candidate voice ports but did not recognize an IP-discovery or STUN Binding request. These counters are system-wide and do not identify the process that sent the packets. The log alone cannot establish whether the app did not send discovery, whether its voice session stopped before that stage, or whether the test was mixed with other network activity. The zero UDP/443 candidate count is only an observation for this interval; it does not establish that QUIC caused the voice failure.

Verbatim snapshots supplied by the user:

```text
[2026-10-31 19:59:42.562] [scpdpi] [info] ScpDPI service initialized with profile default
[2026-10-31 19:59:43.192] [scpdpi] [info] DPI packet processor started
[2026-10-31 19:59:43.195] [scpdpi] [info] Diagnostic snapshot: interfaces=4 routes=32 vpn_candidates=1 dpi_enabled=true dpi_running=true dpi_error= packets=0 tcp_tls_candidates=0 tcp_tls_tunnel_candidates=0 tls_parsed=0 tls_unlisted=0 tls_timeouts=0 listed_tls_matches=0 discord_tls_matches=0 youtube_tls_matches=0 discord_tls_rx=0 discord_tls_alerts=0 discord_tls_resets=0 discord_tls_response_timeouts=0 split_packets=0 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=0 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 youtube_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_responses=0 stun_binding_requests=0 stun_binding_success_responses=0 stun_binding_error_responses=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=2 strategy_changes=0
[2026-10-31 20:00:43.354] [scpdpi] [info] Diagnostic snapshot: interfaces=3 routes=23 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=3030 tcp_tls_candidates=1707 tcp_tls_tunnel_candidates=202 tls_parsed=58 tls_unlisted=45 tls_timeouts=0 listed_tls_matches=16 discord_tls_matches=14 youtube_tls_matches=0 discord_tls_rx=13 discord_tls_alerts=0 discord_tls_resets=1 discord_tls_response_timeouts=1 split_packets=62 split_failures=0 send_failures=0 passive_resets_dropped=1 udp443_candidates=17 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 youtube_quic_fallbacks=0 voice_udp_port_candidates=18 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_responses=0 stun_binding_requests=0 stun_binding_success_responses=0 stun_binding_error_responses=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=1 strategy_changes=1
[2026-10-31 20:04:01.544] [scpdpi] [info] ScpDPI service initialized with profile default
[2026-10-31 20:04:02.177] [scpdpi] [info] DPI packet processor started
[2026-10-31 20:04:02.180] [scpdpi] [info] Diagnostic snapshot: interfaces=4 routes=32 vpn_candidates=1 dpi_enabled=true dpi_running=true dpi_error= packets=0 tcp_tls_candidates=0 tcp_tls_tunnel_candidates=0 tls_parsed=0 tls_unlisted=0 tls_timeouts=0 listed_tls_matches=0 discord_tls_matches=0 youtube_tls_matches=0 discord_tls_rx=0 discord_tls_alerts=0 discord_tls_resets=0 discord_tls_response_timeouts=0 discord_media_tls_matches=0 discord_media_tls_rx=0 discord_media_tls_alerts=0 discord_media_tls_resets=0 discord_media_tls_response_timeouts=0 youtube_tls_rx=0 youtube_tls_alerts=0 youtube_tls_resets=0 youtube_tls_response_timeouts=0 split_packets=0 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=0 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 youtube_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_responses=0 stun_binding_requests=0 stun_binding_success_responses=0 stun_binding_error_responses=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=2 strategy_changes=0
[2026-10-31 20:04:09.065] [scpdpi] [info] ScpDPI service initialized with profile default
[2026-10-31 20:04:09.688] [scpdpi] [info] DPI packet processor started
[2026-10-31 20:04:09.690] [scpdpi] [info] Diagnostic snapshot: interfaces=2 routes=17 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=0 tcp_tls_candidates=0 tcp_tls_tunnel_candidates=0 tls_parsed=0 tls_unlisted=0 tls_timeouts=0 listed_tls_matches=0 discord_tls_matches=0 youtube_tls_matches=0 discord_tls_rx=0 discord_tls_alerts=0 discord_tls_resets=0 discord_tls_response_timeouts=0 discord_media_tls_matches=0 discord_media_tls_rx=0 discord_media_tls_alerts=0 discord_media_tls_resets=0 discord_media_tls_response_timeouts=0 youtube_tls_rx=0 youtube_tls_alerts=0 youtube_tls_resets=0 youtube_tls_response_timeouts=0 split_packets=0 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=0 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 youtube_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_responses=0 stun_binding_requests=0 stun_binding_success_responses=0 stun_binding_error_responses=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=0 strategy_changes=0
[2026-10-31 20:04:17.269] [scpdpi] [info] ScpDPI service initialized with profile default
[2026-10-31 20:04:17.894] [scpdpi] [info] DPI packet processor started
[2026-10-31 20:04:17.896] [scpdpi] [info] Diagnostic snapshot: interfaces=2 routes=17 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=0 tcp_tls_candidates=0 tcp_tls_tunnel_candidates=0 tls_parsed=0 tls_unlisted=0 tls_timeouts=0 listed_tls_matches=0 discord_tls_matches=0 youtube_tls_matches=0 discord_tls_rx=0 discord_tls_alerts=0 discord_tls_resets=0 discord_tls_response_timeouts=0 discord_media_tls_matches=0 discord_media_tls_rx=0 discord_media_tls_alerts=0 discord_media_tls_resets=0 discord_media_tls_response_timeouts=0 youtube_tls_rx=0 youtube_tls_alerts=0 youtube_tls_resets=0 youtube_tls_response_timeouts=0 split_packets=0 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=0 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 youtube_quic_fallbacks=0 voice_udp_port_candidates=0 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_responses=0 stun_binding_requests=0 stun_binding_success_responses=0 stun_binding_error_responses=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=0 strategy_changes=0
[2026-10-31 20:05:18.064] [scpdpi] [info] Diagnostic snapshot: interfaces=3 routes=23 vpn_candidates=0 dpi_enabled=true dpi_running=true dpi_error= packets=1564 tcp_tls_candidates=885 tcp_tls_tunnel_candidates=0 tls_parsed=32 tls_unlisted=24 tls_timeouts=0 listed_tls_matches=14 discord_tls_matches=12 youtube_tls_matches=0 discord_tls_rx=12 discord_tls_alerts=0 discord_tls_resets=0 discord_tls_response_timeouts=0 discord_media_tls_matches=5 discord_media_tls_rx=5 discord_media_tls_alerts=0 discord_media_tls_resets=0 discord_media_tls_response_timeouts=0 youtube_tls_rx=0 youtube_tls_alerts=0 youtube_tls_resets=0 youtube_tls_response_timeouts=0 split_packets=55 split_failures=0 send_failures=0 passive_resets_dropped=0 udp443_candidates=0 udp443_tunnel_candidates=0 quic_fallbacks=0 discord_quic_fallbacks=0 youtube_quic_fallbacks=0 voice_udp_port_candidates=22 voice_udp_tunnel_candidates=0 voice_discovery_requests=0 voice_discovery_direct=0 voice_discovery_tunnel=0 discord_ip_discovery=0 discord_ip_discovery_responses=0 stun_binding_requests=0 stun_binding_success_responses=0 stun_binding_error_responses=0 discord_ip_discovery_other_ports=0 udp_decoys_sent=0 udp_decoys_failed=0 tunnel_interfaces=1 strategy_changes=0
```

### Follow-up verification

- This was a log/report-only update; no source or installed binary changed.
- The counters still cannot distinguish Discord desktop from browser or other processes. A clean diagnostic run should start the service once, test only the desktop app, and capture the next periodic snapshot without restarting the service.
