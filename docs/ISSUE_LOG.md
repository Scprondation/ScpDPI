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

## References

- [GoodbyeDPI official repository and behavior description](https://github.com/ValdikSS/GoodbyeDPI)
- [GoodbyeDPI published license](https://github.com/ValdikSS/GoodbyeDPI/blob/master/LICENSE)
- [Discord: Ошибки голосового соединения](https://support.discord.com/hc/ru/articles/115001310031-%D0%9E%D1%88%D0%B8%D0%B1%D0%BA%D0%B8-%D0%B3%D0%BE%D0%BB%D0%BE%D1%81%D0%BE%D0%B2%D0%BE%D0%B3%D0%BE-%D1%81%D0%BE%D0%B5%D0%B4%D0%B8%D0%BD%D0%B5%D0%BD%D0%B8%D1%8F)
