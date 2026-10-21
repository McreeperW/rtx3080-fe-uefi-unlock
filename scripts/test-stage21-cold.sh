#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EDK="$ROOT/deps/edk2/MdePkg/Include"
[[ -f "$EDK/Uefi.h" ]] || { echo 'MdePkg Uefi.h not found' >&2; exit 2; }
command -v clang >/dev/null || { echo 'clang not found' >&2; exit 3; }
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
clang -fshort-wchar -I "$EDK" -I "$EDK/X64"  "$ROOT/fe/stage21/tests/test_cold.c" -o "$TMP/test_cold"
"$TMP/test_cold"
echo 'PASS: Stage21 cold state C unit test (source only, no GPU I/O)'
