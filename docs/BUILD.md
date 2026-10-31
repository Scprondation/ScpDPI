# Build ScpDPI

## Requirements

- CMake 3.24 or later and Ninja.
- C++20 compiler: Visual Studio 2022/2026 MSVC, GCC 11+, or Clang 14+.
- vcpkg with `VCPKG_ROOT` set. The base manifest installs nlohmann/json, spdlog and GoogleTest.
- Qt 6 Quick and QML tooling are needed only for the optional desktop target. The normal CMake configuration and `windows-console-*` presets build Core, Service and tests without Qt.

## Windows 10/11 x64

Open a Developer PowerShell prompt matching the installed Visual Studio toolchain:

```powershell
$env:VCPKG_ROOT = 'C:\src\vcpkg'
cmake --preset windows-console-debug
cmake --build --preset windows-console-debug
ctest --preset windows-console-debug
```

For Release, replace `windows-console-debug` with `windows-console-release`. Windows interface and route discovery use IP Helper APIs and do not require shelling out to `ipconfig` or `route`.

The console presets do not select the Qt manifest feature. They are also the default in the PowerShell build script:

```powershell
cmake --preset windows-console-debug
cmake --build --preset windows-console-debug
ctest --preset windows-console-debug
```

For Release, use `windows-console-release`. The executable is `out\build\windows-console-debug\src\service\scpdpi_service.exe`; run it directly or pass `--console` for the interactive interface. The `scpdpi_autostart.exe` helper is built alongside it.

To run the interactive console without Qt, build the Core and Service targets, then start `scpdpi_service.exe`. Its first Windows launch installs a per-user copy and registers a current-user startup entry. The helper starts the scanner without opening a window; closing the console leaves the scanner running. Type `autostart on` or `autostart off` in the console, or run `scpdpi_service.exe --enable-autostart` / `--remove-autostart` to change sign-in startup. These operations do not require administrator privileges.

Use `scpdpi_service.exe --snapshot` for a one-time JSON report. Linux automatic startup is deferred; its console currently runs in the foreground.

The Windows helper script builds that configuration by default; add `-Configuration Release` for a release build. Pass `-WithDesktop` to select the optional Qt presets.

## Linux (Ubuntu 22.04/24.04)

Install the compiler, CMake, Ninja, and vcpkg prerequisites using your distribution package manager, then:

```sh
export VCPKG_ROOT="$HOME/src/vcpkg"
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

For Release, use `linux-release`. The Linux backend uses `getifaddrs`, ioctl metadata, and RTNETLINK. It does not run `ip`, `ifconfig`, or other shell commands to build the scan result.

## Presets and optional targets

Presets use Ninja and vcpkg manifest mode. Set `VCPKG_ROOT` before configuring so `CMakePresets.json` can find `scripts/buildsystems/vcpkg.cmake`. To build Core, Service and tests without Qt, use:

```sh
cmake -S . -B out/core -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DSCPDPI_BUILD_DESKTOP=OFF
cmake --build out/core
```

For Clang, set `CMAKE_CXX_COMPILER=clang++` at configure time in a separate build directory. Do not reuse a CMake build directory after changing compilers.

## Troubleshooting

- `CMAKE_TOOLCHAIN_FILE` missing: check that `VCPKG_ROOT` points to a vcpkg checkout.
- Qt target unavailable: install the manifest dependencies and check that vcpkg selected a triplet with Qt Quick/QML.
- Linux route dump permission: route and interface reads should work as the current user; ScpDPI does not need to elevate itself.
- Native platform code has only been verified when the corresponding build is run on that OS. Do not interpret a generated build directory or successful configure as a completed cross-platform build.

## Verification status

The Windows console Release preset has configured and built with MSVC, and CTest passes all 26 GoogleTest cases. The console was run against live Windows interface and route data; a first-run integration check confirmed the per-user install and startup entry, then verified that the background process remained after the console exited. Qt desktop compilation and Linux compilation are deferred in this workspace; the Linux backend source and Qt/QML source are still present.
