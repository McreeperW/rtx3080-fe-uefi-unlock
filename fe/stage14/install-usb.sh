#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
USB="${1:-}"
if [[ ! "$USB" =~ ^/mnt/[a-z]$ || ! -f "$USB/EFI/BOOT/BOOTX64.EFI" || ! -f "$HERE/out/BOOTX64.EFI" ]]; then
  echo 'Usage: bash fe/stage14/install-usb.sh /mnt/f' >&2
  echo 'Mount and verify the removable FAT32 USB, NOT the Windows system disk.' >&2
  exit 2
fi
backup="$USB/EFI/BOOT/BOOTX64.STAGE14-BACKUP.EFI"
if [[ -e "$backup" ]]; then
  echo "Refusing to overwrite existing Stage14 backup: $backup" >&2; exit 3
fi
cp "$USB/EFI/BOOT/BOOTX64.EFI" "$backup"
cp "$HERE/out/BOOTX64.EFI" "$USB/EFI/BOOT/BOOTX64.EFI"
sync
cmp "$HERE/out/BOOTX64.EFI" "$USB/EFI/BOOT/BOOTX64.EFI"
echo 'PASS: Stage14 read-only boot bridge installed; earlier EFI backed up.'
sha256sum "$USB/EFI/BOOT/BOOTX64.EFI"
