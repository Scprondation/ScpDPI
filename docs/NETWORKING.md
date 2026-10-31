# Networking behavior

ScpDPI keeps two functions separate. The network scanner reads interfaces and routes for diagnostics. The Windows DPI backend processes selected HTTP/TLS flows for domains in the bundled lists; it does not create a VPN or encrypt traffic.

## Windows DPI backend

ScpDPI owns the packet-processing logic. It captures HTTP/TLS candidate TCP ports (80, 443, 2053, 2083, 2087, 2096 and 8443) and selected outbound UDP traffic with the pinned WinDivert driver. The UDP filter selects the fixed 74-byte Discord IP-discovery signature across arbitrary destination ports, because Discord supplies the voice server's UDP port dynamically. The filter checks the message length, type, payload length, and nonzero SSRC; ScpDPI validates the full request before handling it. It holds a candidate TLS ClientHello only long enough to parse SNI; a host in the Russian registry, YouTube or Discord list gets one of three ScpDPI-specific segmentation and delivery-order patterns. The patterns preserve the original byte stream and TCP sequence space. An inbound reset, TLS alert or response timeout advances the per-host choice, including after a previously successful pattern. Unknown or unlisted traffic is passed through unchanged.

For UDP/443 on a direct route with no recognized tunnel, ScpDPI decrypts QUIC v1/v2 Initial packets locally and drops only packets whose ClientHello SNI matches a listed host, prompting TCP fallback. Unknown QUIC, unlisted SNI, unsupported QUIC versions, and all UDP/443 on a recognized VPN path pass unchanged. For direct UDP voice traffic, ScpDPI recognizes the fixed Discord IP-discovery request on any destination port and STUN Binding Requests on selected WebRTC ports. It sends at most one protocol-shaped discovery decoy per request identity with a distinct SSRC or STUN transaction ID, then forwards the unchanged original request. Voice media and all other UDP datagrams pass unchanged. Decoys are not inserted on a packet routed through a recognized VPN interface.

The packet driver requires administrator privileges. The logged-in Windows account must belong to Administrators. On first launch ScpDPI asks for UAC approval, installs its versioned build and runtime under Program Files, creates a hidden elevated logon task for that account, and starts the background service. New builds install side-by-side and update the task after stopping the previous ScpDPI service. Each time the console opens, it stops and starts the background service through Task Scheduler, which reloads the packet engine and current tunnel interfaces; closing the console leaves the service running. `dpi on|off|status` controls the packet processor. `autostart on|off` enables or disables the task itself.

The WinDivert archive and Russian registry domain data are pinned and SHA-256 checked at build configuration time. These are fixed protocol-specific strategies, not machine learning. A discovery decoy can help only when a network filter treats the first UDP request differently from the real request; it cannot provide UDP transport, and it cannot repair a network that blocks all UDP. Discord itself says voice needs a VPN that carries UDP when the direct network blocks it ([voice connection troubleshooting](https://support.discord.com/hc/en-us/articles/115001310031-Voice-Connection-Errors)). No strategy is guaranteed to work with every ISP, DPI appliance, VPN, browser, antivirus or network driver. The user can turn it off from the console if it disrupts other traffic.

If a site still does not load, check whether its hostname resolves correctly and whether the ISP block is DNS poisoning, IP blocking, throttling, or an application restriction. ScpDPI does not redirect DNS or tunnel traffic. Browser DNS-over-HTTPS or a separate VPN may be needed for those cases.

After ScpDPI segments a TLS ClientHello for a listed host, it suppresses an inbound IPv4 TCP reset only when the reset belongs to that active connection attempt and its IP ID is 0 or 1, a common passive-DPI signature on some Russian networks. A suppressed reset leaves the attempt open for a real server response; other resets advance the existing per-host strategy and pass through. Resets outside that narrow match pass through. IPv6 resets and IPv4 resets with other IP IDs are not filtered. The count appears in `dpi status` and in periodic diagnostic log entries.

For UDP/443, the egress interface of each packet determines whether it is direct or tunneled. The presence of an unrelated tunnel adapter does not disable QUIC processing on direct interfaces.

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

Detection combines OS virtual/tunnel type, interface name/description hints and routes using the interface. Teredo is reported as a system tunnel, not a VPN candidate. Confidence describes evidence for a tunnel-like interface, not a protocol. VLESS + Reality, Hysteria 2, Xray proxy mode and other user-space proxies may leave no dedicated interface and cannot be identified through this module. Process inspection/integration is not implemented.

## Diagnostics

Diagnostics report absent defaults/addresses, multiple default routes, low MTU metadata, unknown virtual interfaces and possible gateway/route conflicts. Findings can be false positives and are never repaired automatically. The displayed active default is the route with the lowest observed metric; platform-specific policy can select another route.
