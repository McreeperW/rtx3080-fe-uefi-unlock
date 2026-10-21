#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
for c in 15 20 21;do test -d "fe/stage${c}/src" -o -d "fe/stage${c}/engine" || { echo "Missing Stage ${c} source"; exit 1;};done
! find . -type f \( -iname '*.efi' -o -iname '*.bin' -o -iname '*.obj' \) -print -quit | grep -q . || { echo 'ERROR: generated binaries included in source repo';exit 2; }
python3 - <<'PY2'
from pathlib import Path
s=Path('fe/stage21/engine/fe_cold.h').read_text()
a=Path('fe/stage20/src/bridge.c').read_text()
b=Path('fe/stage15/src/bootbridge.c').read_text()
assert '0x10140014U' in s and '0x10341014U' in s
assert 'stage20_reconnect_skipped=' in a and 'bus!=0x26U' in a
assert 'I_ACCEPT_GPU_HANG_RISK' in b and 'FAILED_CHILD_RETURNED' in b
print('PASS: Stage21 cold state, Stage20 headless bridge, Stage15 one-shot source checks')
PY2
