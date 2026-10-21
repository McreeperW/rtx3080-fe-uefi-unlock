#!/usr/bin/env bash
# 연구용 소스 빌드만 수행. GPU 접근, USB 쓰기, 일회성 마커 생성 없음.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
for tool in clang lld-link sha256sum python3; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "ERROR: missing required tool: $tool" >&2; exit 2
  fi
done
if [[ ! -f deps/edk2/MdePkg/Include/Uefi.h ]]; then
  echo 'ERROR: missing EDK2 MdePkg headers' >&2; exit 2
fi
printf '\n[1/4] Stage21 CPU-only cold-state test\n'
bash scripts/test-stage21-cold.sh
printf '\n[2/4] Stage15 boot bridge\n'
bash fe/stage15/build.sh
printf '\n[3/4] Stage20 no-GOP-reconnect bridge\n'
bash fe/stage20/build.sh
printf '\n[4/4] Stage21 firmware unlock engine\n'
bash fe/stage21/build-engine.sh
python3 - <<'PY'
from pathlib import Path
import hashlib
items = [
 ('Stage15',Path('fe/stage15/out/BOOTX64.EFI'),'e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00'),
 ('Stage20',Path('fe/stage20/out/RTX3080-UNLOCK.EFI'),'8ef922e0804bf5b0d68f028e8af2c77b676ebfb9b0b3307b7311516e8747d4c5'),
 ('Stage21',Path('fe/stage21/out/RTX3080-STAGE19-ENGINE.EFI'),'0d578a0612fb8a59b7320a0e038d2f19b03f9573b2a995f10ed44e6e10b32373')]
print('\n=== Build output hash comparison (observed successful PC only) ===')
all_match = True
for label, path, expected in items:
    actual=hashlib.sha256(path.read_bytes()).hexdigest()
    status='MATCH' if actual==expected else 'DIFF / NOT TESTED'
    print(f'{label:8s} {status}\n  file={path}\n  sha256={actual}')
    all_match &= (actual == expected)
print('\n'+('All files byte-identical to observed build. STILL NOT VERIFIED ON OTHER HARDWARE.' if all_match else 'Compilation succeeded, but one or more files differ. DO NOT assume a safe/validated USB build.'))
PY
printf '\nDONE: source build only. No USB changes or GPU operations.\n'
