# Roadmap

## 0.1.0

- Read-only native interface and route inventory.
- Tunnel evidence, route-based classification, safe policy defaults, diagnostics, configuration, telemetry, desktop and local IPC foundation.
- No packet capture or network changes.

## 0.2.0 candidates

- Parse Linux routing policy rules and preserve route table IDs so route classification can explain policy-table selection.
- Add Windows interface metric to effective route ranking and test against multiple active defaults.
- Add event-driven refresh (Windows IP Helper notifications and Linux netlink subscriptions) with polling fallback.
- Add authenticated/versioned service lifecycle and GUI-to-service client with reconnect/error states.
- Add profile schema loading and editing with migration tests.
- Add process metadata integrations behind consented, least-privilege providers; do not inspect VPN secrets.
- Add CI build/test jobs for MSVC, GCC, Clang, Qt and Linux/Windows.
- Render and review the QML app at common desktop sizes and add accessibility/keyboard tests.

Packet interception and automatic route/DNS changes require a separate threat model and explicit product review before implementation.
