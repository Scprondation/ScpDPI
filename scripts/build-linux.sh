#!/usr/bin/env sh
set -eu
if [ -z "${VCPKG_ROOT:-}" ]; then
  echo "Set VCPKG_ROOT to your vcpkg checkout before building." >&2
  exit 2
fi
configuration="${1:-Debug}"
case "$configuration" in
  Debug) preset=linux-debug ;;
  Release) preset=linux-release ;;
  *) echo "Usage: $0 [Debug|Release]" >&2; exit 2 ;;
esac
cmake --preset "$preset"
cmake --build --preset "$preset"
if [ "$configuration" = Debug ]; then ctest --preset "$preset"; fi
