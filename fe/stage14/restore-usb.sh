#!/usr/bin/env bash
set -euo pipefail
USB="${1:-}"
if [[ ! "$USB" =~ ^/mnt/[a-z]$ || ! -f "$USB/EFI/BOOT/BOOTX64.STAGE14-BACKUP.EFI" ]]; then
  echo 'Usage: bash fe/stage14/restore-usb.sh /mnt/f' >&2; exit 2
fi
cp "$USB/EFI/BOOT/BOOTX64.STAGE14-BACKUP.EFI" "$USB/EFI/BOOT/BOOTX64.EFI"
sync
echo 'PASS: previous Stage13/13b boot EFI restored.'
