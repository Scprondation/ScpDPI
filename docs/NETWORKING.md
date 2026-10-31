# Networking behavior

ScpDPI 0.1.0 is observational. It does not capture payloads, inspect encrypted traffic, create a TUN interface, start another VPN, or modify DNS, route metrics, routes or MTU. Unknown or unresolved paths use the Bypass policy.

## Interface discovery

- Windows: `GetAdaptersAddresses(AF_UNSPEC)` supplies names, description, state, MTU, MAC, type and assigned IPv4/IPv6 addresses.
- Linux: `getifaddrs` supplies addresses and flags; AF_PACKET and ioctl provide link address and MTU. Virtual/physical is a best-effort classification based on link type, flags and interface metadata.

Physical vs virtual is a heuristic. OS adapter categories and driver descriptions differ, and bridges, containers, hypervisors and proxy clients can all create virtual interfaces.

## Route discovery and classification

- Windows: `GetIpForwardTable2(AF_UNSPEC)` supplies IPv4 and IPv6 route rows.
- Linux: RTNETLINK route dumps supply unicast rows from the kernel's main/default/local tables. Routing rules and process-specific marks are not resolved.

The classifier performs longest-prefix match on the available route rows and uses the route metric to break ties. VPN classification requires the selected interface to be present in the VPN evidence list. An interface with no active/recognized route, a missing interface row, or no matching route yields `Unknown`.

Because policy routing, multiple tables, per-process VPNs, source-specific routes and socket marks affect Linux route choice, destination-only matching is a diagnostic approximation. Windows compartment and source-address choices can also affect a real connection. ScpDPI reports observed data; it does not claim to know the path selected for every process.

## VPN detection

Detection combines OS virtual/tunnel type, interface name/description hints and routes using the interface. Confidence describes evidence for a tunnel-like interface, not a protocol. VLESS + Reality, Hysteria 2, Xray proxy mode and other user-space proxies may leave no dedicated interface and cannot be identified through this module. Process inspection/integration is Not Implemented.

## Diagnostics

Diagnostics report absent defaults/addresses, multiple default routes, low MTU metadata, unknown virtual interfaces and possible gateway/route conflicts. Findings can be false positives and are never repaired automatically. The displayed active default is the route with the lowest observed metric; platform-specific policy can select another route.
