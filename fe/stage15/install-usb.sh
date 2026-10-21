#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
USB="${1:-}"
[[ -n "$USB" && -d "$USB/EFI/BOOT" && -f "$USB/EFI/BOOT/BOOTX64.EFI" ]] || { echo 'Usage: install-usb.sh /mnt/f  (validated USB only)' >&2; exit 2; }
[[ -f "$ROOT/out/BOOTX64.EFI" ]] || exit 2
hash=$(sha256sum "$ROOT/out/BOOTX64.EFI" | cut -d' ' -f1)
[[ "$hash" == e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00 ]] || { echo "Stage15 EFI file was modified: $hash" >&2; exit 3; }
current=$(sha256sum "$USB/EFI/BOOT/BOOTX64.EFI" | cut -d' ' -f1)
if [[ "$current" != e3f4de50a3991962158b645c977dbdf9a5a9db5afd964c3bf1b324efc9ffbbb4 ]]; then
  echo "REFUSED: expected exact Stage14 validation EFI at USB (found $current)" >&2; exit 4
fi
backup="$USB/EFI/BOOT/BOOTX64.STAGE15-BACKUP.EFI"
[[ ! -e "$backup" ]] || { echo "REFUSED: backup already present: $backup" >&2; exit 5; }
cp -p "$USB/EFI/BOOT/BOOTX64.EFI" "$backup"
cp "$ROOT/out/BOOTX64.EFI" "$USB/EFI/BOOT/BOOTX64.EFI"
sync
check=$(sha256sum "$USB/EFI/BOOT/BOOTX64.EFI" | cut -d' ' -f1)
[[ "$check" == "$hash" ]] || { echo 'USB copy verification failed' >&2; exit 6; }
echo "PASS: Stage15 headless bridge installed at $USB; no unlock module or arm marker installed."
echo "Stage14 backup: $backup"
