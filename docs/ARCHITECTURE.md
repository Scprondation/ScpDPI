# Architecture

ScpDPI is divided into a Qt-free Core, a foreground service host, and a Qt Quick desktop client. The Core has no dependency on Qt. Native APIs live behind `IInterfaceProvider` and `IRouteProvider`, which also lets tests inject in-memory providers.

```text
Windows IP Helper ─┐                       ┌─ QML dashboard
                   ├─ providers ─ Core ────┤  (Core API in-process)
Linux getifaddrs / ┘              │        └─ profile and diagnostics views
Linux RTNETLINK                   │
                                  └─ service host ─ local IPC
```

## Scan pipeline

1. The provider returns interface and route snapshots.
2. `VpnDetector` correlates virtual/tunnel metadata with routes. It returns a confidence and evidence list, and never identifies an encrypted protocol from interface names.
3. `TrafficClassifier` selects the longest matching prefix, then lower metric for equal prefixes. It maps a route through a detected VPN interface to `Vpn`, a usable non-virtual interface to `Direct`, and everything else to `Unknown`.
4. `PolicyEngine` evaluates active rules in descending priority. A missing match always returns Bypass.
5. `Diagnostics` reports observable conditions and potential conflicts. It does not remediate them.
6. `TelemetryCollector` records aggregate counts and scan duration only.

The desktop calls the Core API and only presents its results. It does not call IP Helper, getifaddrs or RTNETLINK itself. The service shares the same Core library and exposes a narrow command protocol (`status`, `scan`, `start`, `stop`, `set_profile`, `reload_config`, `enable_dpi`, `disable_dpi`, `enable_autostart`, `disable_autostart`). IPC payloads are JSON, capped at 16 KiB, and validated against an allowlist.

On Windows the service owns ScpDPI's in-process packet engine. WinDivert provides packet capture and reinjection; ScpDPI itself parses TLS ClientHello/SNI, matches its Russian domain lists, chooses per-host segmentation and TCP delivery-order patterns, and recalculates packet checksums. Selected TLS ports are inspected on physical and recognized tunnel interfaces. The QUIC v1/v2 Initial parser derives client Initial keys, decrypts the packet in memory, and extracts SNI; only a listed host on a direct route without a recognized tunnel is dropped to encourage TCP fallback. Unknown QUIC and recognized VPN paths are passed through. ScpDPI recognizes Discord's fixed-length IP-discovery payload signature across UDP destination ports and STUN Binding Requests on selected ports; only a fully validated discovery request can trigger a single altered decoy before the original request on direct egress. It never modifies voice media or unknown UDP payloads. The elevated task and versioned runtime live under Program Files; the interactive console uses a local pipe and restarts the background service when opened. The per-user config controls the packet engine and scanner separately.

## State and threading

Provider objects are injected as `shared_ptr<const ...>` and are stateless. The service runs periodic scans on a `std::jthread`; the IPC handler only supports fixed operations. Configuration and snapshots are copied at boundaries. ScpDPI does not change routes, DNS servers or MTU.

## Current limits

Protocol integrations are represented by interfaces and explicit unavailable implementations. Windows VPN process detection is not implemented. Linux route discovery reads kernel route dumps; policy-rule based path selection, per-socket marks, network namespaces and source-address selection are not resolved by the destination-only classifier. The classifier is therefore a useful view of route metadata, not a promise about every process's actual egress path.
