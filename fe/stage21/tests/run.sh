#!/usr/bin/env bash
set -euo pipefail
D=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
H21=0d578a0612fb8a59b7320a0e038d2f19b03f9573b2a995f10ed44e6e10b32373
BIN="$D/out/RTX3080-STAGE19-ENGINE.EFI"
[[ "$(sha256sum "$BIN" | cut -d' ' -f1)" == "$H21" ]]
file "$BIN" | grep -q 'PE32+ executable for EFI (application), x86-64'
bash -n "$D/build-engine.sh" "$D/install-once.sh" "$D/restore-stage20.sh"
# Unit test exact expanded allowlist and fail-closed alternative fields.
EDK="${PROJECT_ROOT:-$(cd "$D/../.." && pwd)}/deps/edk2/MdePkg/Include"
if [[ -f "$EDK/Uefi.h" ]]; then
  TMP=$(mktemp -d)
  trap 'rm -rf "$TMP"' EXIT
  clang -fshort-wchar -I "$EDK" -I "$EDK/X64" "$D/tests/test_cold.c" -o "$TMP/test_cold"
  "$TMP/test_cold"
else
  echo 'ERROR: UEFI MdePkg headers unavailable for guard unit test' >&2
  exit 2
fi
python3 - "$D/engine/app.c" "$D/engine/fe_cold.h" <<'PY_CHECK'
from pathlib import Path
import sys
app,cold=[Path(x).read_text() for x in sys.argv[1:]]
assert 'STAGE21 cold detail' in app
assert '0x10140014U' in cold
assert 's->wpr2_hi==0U' in cold
assert 's->feature_plm==0xffffff8fU' in cold
assert 's->ss1==7U' in cold
assert 's->plm_1183a4==0x2800U' in cold
assert 'a.signature_slots_supported' in cold
assert 'GuSafeToReturn' in app or 'Gu.retained' in app
print('PASS: bounded new cold-state allowlist, full field diagnostics, original stop policy')
PY_CHECK
# Fail-closed: installer on an empty USB folder must not arm or copy anything.
T=$(mktemp -d)
mkdir -p "$T/EFI/BOOT"
if "$D/install-once.sh" "$T" >/dev/null 2>&1;then echo 'ERROR: empty USB accepted'; exit 1; fi
[[ ! -e "$T/RTX3080-STAGE15-ARM.ONCE" ]]
rm -rf "$T"
echo 'PASS: Stage21 PE x64, cold-state unit tests, fail-closed installer.'
