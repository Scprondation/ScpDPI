# Networking behavior

ScpDPI keeps two functions separate. The network scanner reads interfaces and routes for diagnostics. The Windows DPI backend processes selected HTTP/TLS flows for domains in the bundled lists; it does not create a VPN or encrypt traffic.

## Windows DPI backend

ScpDPI owns the packet-processing logic. It captures outbound and inbound TCP/443 plus outbound UDP/443 with the pinned WinDivert driver. For TCP connections, including those routed through a recognized VPN tunnel, it buffers a TLS ClientHello, reads SNI, matches the Russian registry list or the explicit YouTube and Discord lists, then resegments the ClientHello at SNI-dependent boundaries. Unknown or unlisted TLS traffic passes through unchanged. It drops outbound UDP/443 on the selected route to encourage TCP fallback. When a tunnel is detected, physical UDP/443 is passed through to avoid disrupting the VPN's own transport; UDP/443 routed inside the tunnel is dropped.

The packet driver requires administrator privileges. The logged-in Windows account must belong to Administrators. On first launch ScpDPI asks for UAC approval, installs its versioned build and runtime under Program Files, creates a hidden elevated logon task for that account, and starts the background service. New builds install side-by-side and update the task after stopping the previous ScpDPI service. Each time the console opens, it stops and starts the background service through Task Scheduler, which reloads the packet engine and current tunnel interfaces; closing the console leaves the service running. `dpi on|off|status` controls the packet processor. `autostart on|off` enables or disables the task itself.

The WinDivert archive and Russian registry domain data are pinned and SHA-256 checked at build configuration time. ScpDPI rotates through three SNI segmentation patterns per host when it observes a reset, TLS alert, or a response timeout, and retains a pattern after a server response. This is adaptive packet handling, not machine learning. Other UDP traffic, including Discord voice media, is not modified; Discord notes that voice connections require a VPN that carries UDP when the direct network blocks it ([voice connection troubleshooting](https://support.discord.com/hc/en-us/articles/115001310031-Voice-Connection-Errors)). No strategy is guaranteed to work with every ISP, DPI appliance, VPN, browser, antivirus or network driver. The user can turn it off from the console if it disrupts other traffic.

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
