#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
USB="${1:-}"
ACK="${2:-}"
if [[ "$ACK" != "I_ACCEPT_GPU_RISK" ]]; then
  echo "Refusing USB installation without explicit I_ACCEPT_GPU_RISK argument" >&2
  exit 2
fi
# Only accept an existing WSL-mounted Windows removable drive root.
USB="${USB%/}"
if [[ ! "$USB" =~ ^/mnt/[d-z]$ || ! -d "$USB/EFI/BOOT" ]]; then
  echo "Pass existing WSL USB drive root, e.g. /mnt/e (EFI/BOOT must exist)." >&2
  exit 3
fi
BIN="$ROOT/fe/stage12/build/BOOTX64.FE-EXPERIMENTAL.EFI"
if [[ ! -s "$BIN" ]]; then
  echo "Missing compiled experimental EFI: run bash fe/stage12/build-experimental.sh" >&2
  exit 4
fi
python3 - "$ROOT" <<'PYCODE'
import sys,json,hashlib,pathlib
root=pathlib.Path(sys.argv[1]); specs=json.loads((root/'unlock/assets.json').read_text())
for a in specs:
    p=root/'assets'/a['Name']
    if not p.is_file() or p.stat().st_size!=a['Bytes'] or hashlib.sha256(p.read_bytes()).hexdigest()!=a['Sha256']:
        raise SystemExit('Missing or mismatched asset: '+str(p))
print('Five GPU firmware assets verified automatically.')
PYCODE
if [[ -f "$USB/EFI/BOOT/BOOTX64.EFI" && ! -e "$USB/EFI/BOOT/BOOTX64.BEFORE-FE-EFI" ]]; then
  cp "$USB/EFI/BOOT/BOOTX64.EFI" "$USB/EFI/BOOT/BOOTX64.BEFORE-FE-EFI"
  echo "Previous EFI backed up on USB."
fi
mkdir -p "$USB/RTX3080-UNLOCK/ASSETS"
for f in booter_load_ga102.bin v67-ga102-610.43.03.bin gsp_ga10x.bin gsp_boot_ga102.bin gsp_boot_desc_ga102.bin; do
  cp "$ROOT/assets/$f" "$USB/RTX3080-UNLOCK/ASSETS/$f"
done
cp "$BIN" "$USB/EFI/BOOT/BOOTX64.EFI"
sync
sha256sum "$USB/EFI/BOOT/BOOTX64.EFI"
echo "USB prepared: $USB  - DO NOT BOOT unless you accept an unvalidated real GPU DMA/FWSEC/FLR experiment."
echo "UEFI requires Secure Boot off; pressing U after preflight initiates a real transaction. ESC cancels."
echo "Log directory: RTX3080-UNLOCK/RUNxx/REPORT.TXT"
