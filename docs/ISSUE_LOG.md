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
- No tests were run. No live Discord browser or RTC test was run either.
- No live Discord browser or RTC test was run. No packet payload capture was taken.
- After the new build is installed and the service has run, capture `dpi status` after trying Discord web once and RTC once, along with `%LOCALAPPDATA%\ScpDPI\scpdpi.log`. Avoid including unrelated personal traffic details when sharing logs.

## References

- [GoodbyeDPI official repository and behavior description](https://github.com/ValdikSS/GoodbyeDPI)
- [GoodbyeDPI published license](https://github.com/ValdikSS/GoodbyeDPI/blob/master/LICENSE)
- [Discord voice connection troubleshooting](https://support.discord.com/hc/en-us/articles/115001310031-Voice-Connection-Errors)
