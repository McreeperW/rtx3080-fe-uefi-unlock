#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
python3 - "$HERE/src/preflight.c" "$HERE/out/RTX3080-UNLOCK.EFI" <<'PY'
from pathlib import Path
import struct,sys,re
s=Path(sys.argv[1]).read_text(); d=Path(sys.argv[2]).read_bytes()
assert 'OpenProtocolInformation(' in s and 'RTX3080-STAGE16-PREFLIGHT.TXT' in s
assert 'return EFI_ACCESS_DENIED;' in s
assert not re.search(r'\b(?:DisconnectController|ConnectController|ResetSystem)\s*\(',s)
assert not re.search(r'(?:Mem|Pci|Io)\s*\.\s*Write\s*\(',s)
assert not re.search(r'\b(?:gpu_unlock_610_unlock|GuWrite32|SetAttributes)\s*\(',s)
assert 'StartImage(' not in s
assert d[:2]==b'MZ'; pe=struct.unpack_from('<I',d,0x3c)[0]
assert d[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',d,pe+4)[0]==0x8664
assert struct.unpack_from('<H',d,pe+24+68)[0]==10
print('PASS: Stage16 read-only child builds as EFI x64 and returns controlled failure')
PY
