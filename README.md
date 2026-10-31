# ScpDPI 0.1.0

ScpDPI is a local Windows network diagnostics and DPI circumvention tool. It reads interface and route metadata, reports tunnel evidence, classifies destinations, and can process selected connections for blocked sites. The console and network scanner work without Qt.

## What is implemented

- C++20 Core with injectable interface and route providers.
- Windows interface discovery through `GetAdaptersAddresses` and route discovery through `GetIpForwardTable2`.
- Linux interface discovery through `getifaddrs` and route discovery through RTNETLINK.
- VPN interface heuristics with evidence and confidence. The result does not assert an encrypted protocol.
- Longest-prefix IPv4/IPv6 route classification and a safe `Unknown → Bypass` policy default.
- Default route, interface, address-family and route-conflict diagnostics.
- Versioned JSON config with schema checks and replace-safe writes.
- Qt 6/QML desktop source, profile selection, network views, diagnostics and local activity log.
- Background service with bounded JSON IPC over a local named pipe or Unix domain socket. On Windows, a hidden elevated task starts it at sign-in.
- Windows DPI packet processing through a pinned GoodbyeDPI/WinDivert runtime, with Russian block lists for YouTube and Discord.
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

On Windows, launching `scpdpi_service.exe` opens the console. The first run asks for administrator approval once, copies the service and pinned DPI runtime to the protected `%ProgramFiles%\ScpDPI` folder, and installs a hidden elevated logon task. Closing the console with its close button or `quit` leaves the background service running. Use `dpi status`, `dpi on`, or `dpi off` to control DPI processing; `autostart on|off` controls its next sign-in launch. The initial dependency download is about 1.1 MB and is SHA-256 checked during configuration.

Use `scpdpi_service --snapshot` to print one scan as JSON. `--serve` is the background mode used by the elevated Windows logon task. Linux remains diagnostics-only for now. The console shows interface and route data, VPN evidence and diagnostics, and supports profile, scan and Windows DPI controls.

`configs/default.json` is the starter configuration. At runtime the service and desktop use the operating system's per-user application config directory so profile changes do not modify files in the source tree.

## Current boundaries

The Windows bypass processes selected HTTP/TLS connections and blocks QUIC so browsers can retry over TCP. It does not create a VPN, encrypt traffic, or change DNS servers, routes or MTU. The upstream DPI engine is pinned to a release candidate because it contains the current YouTube list; the selected ISP and browser still determine whether the strategy works. If DNS is poisoned or the block is not caused by DPI, this feature alone may not restore access. Linux DPI processing, Qt desktop integration, Xray/WireGuard/OpenVPN/Hysteria integrations and process inspection are not implemented. VPN and route reports remain observational.

## Project docs

- [Architecture](docs/ARCHITECTURE.md)
- [Build instructions](docs/BUILD.md)
- [Core API](docs/API.md)
- [Networking behavior and limits](docs/NETWORKING.md)
- [Roadmap](docs/ROADMAP.md)
- [Third-party runtime and licenses](docs/THIRD_PARTY.md)

See [LICENSE](LICENSE).
