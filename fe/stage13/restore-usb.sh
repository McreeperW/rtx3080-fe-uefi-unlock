#!/usr/bin/env bash
set -euo pipefail
USB="${1%/}"
if [[ ! "$USB" =~ ^/mnt/[d-z]$ || ! -f "$USB/RTX3080-UNLOCK/BACKUP/BOOTX64.STAGE12.EFI" || ! -d "$USB/EFI/BOOT" ]];then
  echo 'Missing Stage 12 backup or bad removable USB path' >&2; exit 2
fi
cp "$USB/RTX3080-UNLOCK/BACKUP/BOOTX64.STAGE12.EFI" "$USB/EFI/BOOT/BOOTX64.EFI"
sync
cmp -s "$USB/RTX3080-UNLOCK/BACKUP/BOOTX64.STAGE12.EFI" "$USB/EFI/BOOT/BOOTX64.EFI"
echo 'Restored original Stage 12 EFI onto USB. This EFI can WRITE GPU registers; do not automatically launch.'
