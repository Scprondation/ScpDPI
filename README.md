# ScpDPI 0.1.0

ScpDPI is a local, read only network visibility tool for Windows 10/11 and Linux. It reads interface and route metadata, reports tunnel evidence, classifies destinations against the routes it can see, and produces diagnostics. It does not capture packets or change network behavior.

## What is implemented

- C++20 Core with injectable interface and route providers.
- Windows interface discovery through `GetAdaptersAddresses` and route discovery through `GetIpForwardTable2`.
- Linux interface discovery through `getifaddrs` and route discovery through RTNETLINK.
- VPN interface heuristics with evidence and confidence. The result does not assert an encrypted protocol.
- Longest-prefix IPv4/IPv6 route classification and a safe `Unknown → Bypass` policy default.
- Default route, interface, address-family and route-conflict diagnostics.
- Versioned JSON config with schema checks and replace-safe writes.
- Qt 6/QML desktop source, profile selection, network views, diagnostics and local activity log.
- Background service with bounded JSON IPC over a local named pipe or Unix domain socket. On Windows, the first console launch installs a per-user copy and starts it at sign-in without showing a window.
- GoogleTest coverage for configuration, policy, classification, IPv4/IPv6 and mock providers.

## Build

Install CMake 3.24+, Ninja, a C++20 compiler, and vcpkg. Set `VCPKG_ROOT` to the vcpkg checkout. From a Visual Studio Developer PowerShell prompt on Windows, or a shell with GCC/Clang on Linux:

```powershell
cmake --preset windows-console-debug
cmake --build --preset windows-console-debug
ctest --preset windows-console-debug
```

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Release presets are also available. The default CMake configuration and Windows build script use the console app and do not install Qt. Choose a `windows-debug`/`windows-release` preset or pass `-WithDesktop` to the build script only when you want the optional Qt desktop. See [docs/BUILD.md](docs/BUILD.md) for dependency installation, compiler selection and troubleshooting.

Run the resulting Windows `scpdpi_service.exe` (or pass `--console`) to open the interactive interface. The PowerShell build script builds this console version by default; pass `-WithDesktop` only when you want the optional Qt application.

## Run

When built with the optional Qt desktop feature, the GUI executable is `scpdpi_desktop`. On Windows, launching `scpdpi_service.exe` opens the console. On its first run, ScpDPI copies its service and runtime files into `%LOCALAPPDATA%\Programs\ScpDPI`, adds a current-user startup entry, and starts the background scanner. The startup helper has no visible window. Closing the console with its close button or `quit` only closes the console; the scanner keeps running and starts again at the next sign-in. Manage the entry with `autostart on|off` in the console, or use `scpdpi_service --enable-autostart` / `scpdpi_service --remove-autostart`.

Use `scpdpi_service --snapshot` to print one scan as JSON, or `scpdpi_service --serve` to run the local IPC service directly. On Linux, `--console` runs in the foreground; automatic sign-in startup is currently implemented for Windows only. The console shows interface and route data, VPN evidence and diagnostics, and supports profile and scan controls. It uses the current user's config directory and does not require administrator privileges for discovery.

`configs/default.json` is the starter configuration. At runtime the service and desktop use the operating system's per-user application config directory so profile changes do not modify files in the source tree.

## Current boundaries

ScpDPI 0.1.0 does not intercept packets, alter routes/DNS/MTU, create a TUN device, launch another VPN, or monitor VPN processes. Xray, WireGuard, OpenVPN and Hysteria process integrations are explicitly Not Implemented. Unknown routes are bypassed. Routing table discovery is observational and route classification can only use route and interface data exposed by the operating system; policy routing and per-process route selection can make a destination's actual path differ.

## Project docs

- [Architecture](docs/ARCHITECTURE.md)
- [Build instructions](docs/BUILD.md)
- [Core API](docs/API.md)
- [Networking behavior and limits](docs/NETWORKING.md)
- [Roadmap](docs/ROADMAP.md)

See [LICENSE](LICENSE).
