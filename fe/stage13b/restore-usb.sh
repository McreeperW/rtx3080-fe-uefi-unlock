#!/usr/bin/env bash
set -euo pipefail
USB="${1:-}"
if [[ ! "$USB" =~ ^/mnt/[a-z]$ ]] || [[ ! -f "$USB/EFI/BOOT/BOOTX64.STAGE13-BACKUP.EFI" ]]; then
  echo 'Usage: bash fe/stage13b/restore-usb.sh /mnt/f (requires Stage13 backup)' >&2
  exit 2
fi
cp "$USB/EFI/BOOT/BOOTX64.STAGE13-BACKUP.EFI" "$USB/EFI/BOOT/BOOTX64.EFI"
sync
echo 'Stage 13 read-only EFI restored'
