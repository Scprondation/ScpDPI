param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$ConsoleOnly,
    [switch]$WithDesktop
)
$ErrorActionPreference = 'Stop'
if (-not $env:VCPKG_ROOT) { throw 'Set VCPKG_ROOT to your vcpkg checkout before building.' }
if ($ConsoleOnly -and $WithDesktop) { throw 'Choose either -ConsoleOnly or -WithDesktop, not both.' }
$presetPrefix = if ($WithDesktop) { 'windows' } else { 'windows-console' }
$preset = if ($Configuration -eq 'Release') { "$presetPrefix-release" } else { "$presetPrefix-debug" }
cmake --preset $preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build --preset $preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --preset $preset
exit $LASTEXITCODE
