#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
gcc -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_guard.c" "$HERE/../src/guard.c" -o "$TMP/test_guard"
"$TMP/test_guard"
# hard safety checks must remain true for this build
grep -q 'S14.recovery_proven=0' "$HERE/../src/bootbridge.c"
grep -q 'S14.stage12_integrated_and_verified=0' "$HERE/../src/bootbridge.c"
! grep -Eq 'DisconnectController\(|ResetSystem\(|Pci\.Write\(|Mem\.Write\(|Io\.Write\(' "$HERE/../src/bootbridge.c"
echo 'PASS: Stage14 compiled source contains no GPU writes or reset commands'
