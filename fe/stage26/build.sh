#!/usr/bin/env bash
# Stage26 is a policy update to fe/stage21/engine; it is not a new GPU engine.
# This script builds, tests and labels the modified Stage21 engine without
# installing it or enabling any UEFI unlock attempt.
set -euo pipefail
D=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd -- "$D/../.." && pwd)
APP="$ROOT/fe/stage21/engine/app.c"
COLD="$ROOT/fe/stage21/engine/fe_cold.h"
TEST="$ROOT/fe/stage21/tests/run-stage26.sh"

for tool in clang lld-link file sha256sum cmp; do
  command -v "$tool" >/dev/null 2>&1 || {
    echo "ERROR: missing required command: $tool" >&2; exit 2;
  }
done
[[ -f "$APP" && -f "$COLD" && -f "$TEST" ]] || {
  echo 'ERROR: Stage26 sources/tests missing. Apply Stage26 source update before building.' >&2
  exit 3
}
grep -Fq 'STAGE26 SS0 snapshot' "$APP" &&
grep -Fq 'a.environment_ok_without_ss0?' "$APP" &&
grep -Fq 's->ss0!=0xffffffffU' "$COLD" || {
  echo 'ERROR: expected Stage26 SS0 log-only policy not present. Refusing to label old engine Stage26.' >&2
  exit 4
}

printf '\n[1/3] Compiling Stage26 policy engine via the existing Stage21 build script...\n'
bash "$ROOT/fe/stage21/build-engine.sh"
printf '\n[2/3] Running Stage26 host/mock/PE tests...\n'
bash "$ROOT/fe/stage21/tests/run-stage26.sh"

SRC="$ROOT/fe/stage21/out/RTX3080-STAGE19-ENGINE.EFI"
[[ -s "$SRC" ]] || { echo 'ERROR: expected engine output missing' >&2; exit 5; }
mkdir -p "$D/out"
DST="$D/out/RTX3080-STAGE26-ENGINE.EFI"
cp -- "$SRC" "$DST"
cmp -s -- "$SRC" "$DST" || { echo 'ERROR: copied artifact mismatch' >&2; exit 6; }
file -- "$DST" | grep -q 'PE32+ executable for EFI (application), x86-64' || {
  echo 'ERROR: copied artifact is not an x64 UEFI application' >&2; exit 7;
}
printf '\n[3/3] Stage26 artifact built and verified (NO USB INSTALL):\n'
sha256sum "$SRC" "$DST"
printf 'Source (bridge-compatible filename): %s\n' "$SRC"
printf 'Labeled copy (archive only):      %s\n' "$DST"
printf '\nIMPORTANT: Stage20 expects EFI/BOOT/RTX3080-STAGE19-ENGINE.EFI, not the labeled filename.\n'
printf 'A successful build or mock test does NOT establish hardware safety or boot stability.\n'
