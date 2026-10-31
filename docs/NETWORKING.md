# Networking behavior

ScpDPI keeps two functions separate. The network scanner reads interfaces and routes for diagnostics. The Windows DPI backend processes selected HTTP/TLS flows for domains in the bundled lists; it does not create a VPN or encrypt traffic.

## Windows DPI backend

The Windows build stages the pinned `GoodbyeDPI 0.2.3rc3-2` runtime and WinDivert driver. The default strategy uses the upstream Russian block list, its YouTube list, and ScpDPI's Discord host list. It uses fake TLS requests and blocks QUIC/HTTP3 so browser connections can retry over TCP. The backend does not alter DNS servers, routes or MTU.

The packet driver requires administrator privileges. The logged-in Windows account must belong to Administrators. On first launch ScpDPI asks for UAC approval, installs its backend under the protected Program Files directory, creates a hidden elevated logon task for that account, and starts the background service. The console can then close while the service continues. `dpi on|off|status` controls the packet processor. `autostart on|off` controls the task's sign-in trigger.

The backend is pinned and SHA-256 checked at build configuration time. Its upstream release is a release candidate selected because it includes the current YouTube bypass list. ScpDPI does not promise that one strategy works with every ISP, DPI appliance, VPN, browser, antivirus or network driver. The user can turn it off from the console if it disrupts other traffic.

If a site still does not load, check whether its hostname resolves correctly and whether the ISP block is DNS poisoning, IP blocking, throttling, or an application restriction. ScpDPI does not redirect DNS or tunnel traffic. Browser DNS-over-HTTPS or a separate VPN may be needed for those cases.

## Interface discovery

- Windows: `GetAdaptersAddresses(AF_UNSPEC)` supplies names, description, state, MTU, MAC, type and assigned IPv4/IPv6 addresses.
- Linux: `getifaddrs` supplies addresses and flags; AF_PACKET and ioctl provide link address and MTU. Virtual/physical is a best-effort classification based on link type, flags and interface metadata.

Physical vs virtual is a heuristic. OS adapter categories and driver descriptions differ, and bridges, containers, hypervisors and proxy clients can all create virtual interfaces.

## Route discovery and classification

- Windows: `GetIpForwardTable2(AF_UNSPEC)` supplies IPv4 and IPv6 route rows.
- Linux: RTNETLINK route dumps supply unicast rows from the kernel's main/default/local tables. Routing rules and process-specific marks are not resolved.

The classifier performs longest-prefix match on the available route rows and uses route metric to break ties. VPN classification requires the selected interface to be present in the VPN evidence list. An interface with no active/recognized route, a missing interface row, or no matching route yields `Unknown`.

Because policy routing, multiple tables, per-process VPNs, source-specific routes and socket marks affect Linux route choice, destination-only matching is a diagnostic approximation. Windows compartment and source-address choices can also affect a real connection. ScpDPI reports observed data; it does not claim to know the path selected for every process.

## VPN detection

Detection combines OS virtual/tunnel type, interface name/description hints and routes using the interface. Confidence describes evidence for a tunnel-like interface, not a protocol. VLESS + Reality, Hysteria 2, Xray proxy mode and other user-space proxies may leave no dedicated interface and cannot be identified through this module. Process inspection/integration is not implemented.

## Diagnostics

Diagnostics report absent defaults/addresses, multiple default routes, low MTU metadata, unknown virtual interfaces and possible gateway/route conflicts. Findings can be false positives and are never repaired automatically. The displayed active default is the route with the lowest observed metric; platform-specific policy can select another route.
