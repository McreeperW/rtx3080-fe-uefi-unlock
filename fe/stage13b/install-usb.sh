#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
USB="${1:-}"
if [[ ! "$USB" =~ ^/mnt/[a-z]$ ]] || [[ ! -f "$USB/EFI/BOOT/BOOTX64.EFI" ]] || [[ ! -f "$HERE/out/BOOTX64.EFI" ]]; then
  echo 'Usage: bash fe/stage13b/install-usb.sh /mnt/f' >&2
  echo 'Requires an already-mounted removable USB containing EFI/BOOT/BOOTX64.EFI. Verify drive letter first.' >&2
  exit 2
fi
old="$USB/EFI/BOOT/BOOTX64.STAGE13-BACKUP.EFI"
if [[ ! -e "$old" ]]; then cp "$USB/EFI/BOOT/BOOTX64.EFI" "$old"; fi
cp "$HERE/out/BOOTX64.EFI" "$USB/EFI/BOOT/BOOTX64.EFI"
sync
cmp "$HERE/out/BOOTX64.EFI" "$USB/EFI/BOOT/BOOTX64.EFI"
echo 'PASS: Stage 13b fallback bootbridge copied and byte-compared'
sha256sum "$USB/EFI/BOOT/BOOTX64.EFI"
