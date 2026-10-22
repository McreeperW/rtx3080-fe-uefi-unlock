#!/usr/bin/env bash
set -euo pipefail
D=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
BASE=${PROJECT_ROOT:-"$(cd "$D/../.." && pwd)"}
BIN="$D/out/RTX3080-STAGE26-ENGINE.EFI"
EDK="$BASE/deps/edk2/MdePkg/Include"
[[ -s "$BIN" && -f "$EDK/Uefi.h" ]] || { echo 'ERROR: Stage26 EFI or EDK2 headers missing' >&2; exit 2; }
file "$BIN" | grep -q 'PE32+ executable for EFI (application), x86-64'
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
clang -std=c11 -fshort-wchar -I "$EDK" -I "$EDK/X64" "$D/tests/test_cold.c" -o "$TMP/cold-test"
"$TMP/cold-test"
clang -std=c11 -fshort-wchar -I "$EDK" -I "$EDK/X64" "$D/tests/test_stage26_policy.c" -o "$TMP/stage26-test"
"$TMP/stage26-test"
python3 - "$D/engine/app.c" "$D/engine/fe_cold.h" "$BASE/fe/stage21/engine/app.c" <<'PY'
from pathlib import Path
import sys
app,cold,old=[Path(p).read_text() for p in sys.argv[1:]]
assert 'STAGE26 SS0 snapshot' in app
assert 'a.environment_ok_without_ss0?' in app
assert 's->ss0!=0xffffffffU' in cold
assert 'STAGE26 SS0 snapshot' not in old
assert 'a.known_observation && a.gfw_idle && a.signature_slots_supported?' in old
assert app != old
for guard in ('s->chip==0xb72000a1U','a.gfw_idle','s->wpr2_hi==0U','s->feature_plm==0xffffff8fU','s->ss1==7U','s->plm_1183a4==0x2800U','a.signature_slots_supported'):
    assert guard in cold,guard
print('PASS: Stage26 independent SS0 observation-only policy and other unchanged guards')
PY
sha256sum "$BIN"
