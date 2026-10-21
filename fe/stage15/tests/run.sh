#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
cc -std=c11 -Wall -Wextra -Werror "$HERE/tests/test_guard.c" "$HERE/src/guard.c" -o /tmp/stage15-test-guard-$$
/tmp/stage15-test-guard-$$
rm -f /tmp/stage15-test-guard-$$
python3 - "$HERE/src/bootbridge.c" "$HERE/out/BOOTX64.EFI" <<'PY'
from pathlib import Path
import sys,struct
s=Path(sys.argv[1]).read_text(); d=Path(sys.argv[2]).read_bytes()
assert s.index('load_windows_from_matching_esp') < s.index('run_child_once')
assert s.index('  if(EFI_ERROR(rc)||!windows){out("stage15_result=FAILED_WINDOWS_PRELOAD') < s.index('  run_child_once(image);')
assert s.index('  EFI_STATUS m=take_one_shot_marker(image);') < s.index('  rc=ST->BootServices->StartImage(child,NULL,NULL);')
assert 'I_ACCEPT_GPU_HANG_RISK' in s and 'stage15_result=FAILED_CHILD_RETURNED' in s
assert 'DisconnectController(' not in s and 'ResetSystem(' not in s
assert 'Mem.Write(' not in s and 'Pci.Write(' not in s
assert d[:2]==b'MZ'; pe=struct.unpack_from('<I',d,0x3c)[0]
assert d[pe:pe+4]==b'PE\x00\x00'
assert struct.unpack_from('<H',d,pe+4)[0]==0x8664
assert struct.unpack_from('<H',d,pe+24+68)[0]==10
print('PASS: Stage15 USB preloaded Windows / one-shot / no direct GPU writes / PE32+ EFI x64')
PY
