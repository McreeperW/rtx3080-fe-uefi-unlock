#!/usr/bin/env bash
set -euo pipefail
D=$(cd "$(dirname "$0")/.." && pwd)
BASE=${PROJECT_ROOT:-$(cd "$D/../.." && pwd)}
BIN="$D/out/RTX3080-STAGE19-ENGINE.EFI"
file "$BIN" | grep -q 'PE32+ executable for EFI (application), x86-64'
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
EDK="$BASE/deps/edk2/MdePkg/Include"
clang -std=c11 -fshort-wchar -I "$EDK" -I "$EDK/X64" "$D/tests/test_cold.c" -o "$TMP/legacy-test"
"$TMP/legacy-test"
clang -std=c11 -fshort-wchar -I "$EDK" -I "$EDK/X64" "$D/tests/test_stage26_policy.c" -o "$TMP/stage26-test"
"$TMP/stage26-test"
python3 - "$D/engine/app.c" "$D/engine/fe_cold.h" <<'PY'
from pathlib import Path
import sys
app,cold=(Path(x).read_text() for x in sys.argv[1:])
assert 'Stage="armed-cold-state";status=Step(ColdCheck())' in app
assert 'a.environment_ok_without_ss0?' in app
assert 'STAGE26 SS0 snapshot' in app
assert 'ss0_initial=%#x' in app
assert 'a.known_observation && a.gfw_idle' not in app
assert 's->ss0!=0xffffffffU' in cold
assert 'GuSafeToReturn' in app
for guard in ['s->chip==0xb72000a1U','a.gfw_idle','s->wpr2_hi==0U','s->feature_plm==0xffffff8fU', 's->ss1==7U','s->plm_1183a4==0x2800U','a.signature_slots_supported']:
  assert guard in cold,guard
print('PASS: static cold-state policy, unaltered transaction postconditions')
PY
sha256sum "$BIN"
