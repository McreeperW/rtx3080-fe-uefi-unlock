#!/usr/bin/env bash
set -euo pipefail
D=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
BASE=${PROJECT_ROOT:-"$(cd "$D/../.." && pwd)"}
BIN="$D/out/RTX3080-STAGE19-ENGINE.EFI"
EDK="$BASE/deps/edk2/MdePkg/Include"
[[ -s "$BIN" && -f "$EDK/Uefi.h" ]] || { echo 'ERROR: Stage21 EFI or EDK2 headers missing; build Stage21 first' >&2; exit 2; }
file "$BIN" | grep -q 'PE32+ executable for EFI (application), x86-64'
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
clang -std=c11 -fshort-wchar -I "$EDK" -I "$EDK/X64" "$D/tests/test_cold.c" -o "$TMP/stage21-test"
"$TMP/stage21-test"
python3 - "$D/engine/app.c" "$D/engine/fe_cold.h" <<'PY'
import sys
from pathlib import Path
app,cold=[Path(p).read_text() for p in sys.argv[1:]]
assert 'a.known_observation && a.gfw_idle && a.signature_slots_supported?' in app
assert 'STAGE26 SS0 snapshot' not in app
for ss0 in ('0x10240014U','0x10241014U','0x10341014U','0x10140014U'):
    assert ss0 in cold
assert 'environment_ok_without_ss0' not in cold
print('PASS: Stage21 strict SS0 whitelist, independent source')
PY
sha256sum "$BIN"
