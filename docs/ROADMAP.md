# Roadmap

## 0.1.0

- Native interface and route inventory.
- Tunnel evidence, route-based classification, safe policy defaults, diagnostics, configuration, telemetry, desktop and local IPC foundation.
- Windows DPI processing through a pinned GoodbyeDPI/WinDivert backend, controlled from the console.

## 0.2.0 candidates

- Parse Linux routing policy rules and preserve route table IDs so route classification can explain policy-table selection.
- Add Windows interface metric to effective route ranking and test against multiple active defaults.
- Add event-driven refresh (Windows IP Helper notifications and Linux netlink subscriptions) with polling fallback.
- Add authenticated/versioned service lifecycle and GUI-to-service client with reconnect/error states.
- Add profile schema loading and editing with migration tests.
- Add process metadata integrations behind consented, least-privilege providers; do not inspect VPN secrets.
- Add CI build/test jobs for MSVC, GCC, Clang, Qt and Linux/Windows.
- Render and review the QML app at common desktop sizes and add accessibility/keyboard tests.

Linux DPI processing, backend health reporting, an installer/release package, Windows runtime integration checks and user-configurable target lists remain future work. Automatic DNS, route or MTU changes remain out of scope.
