#!/usr/bin/env bash
set -euo pipefail
USB="${1:-}"
[[ -n "$USB" ]] || { echo 'Usage: restore-usb.sh /mnt/f' >&2; exit 2; }
backup="$USB/EFI/BOOT/BOOTX64.STAGE15-BACKUP.EFI"
[[ -f "$backup" && -f "$USB/EFI/BOOT/BOOTX64.EFI" ]] || exit 2
now=$(sha256sum "$USB/EFI/BOOT/BOOTX64.EFI" | cut -d' ' -f1)
prev=$(sha256sum "$backup" | cut -d' ' -f1)
[[ "$now" == e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00 && "$prev" == e3f4de50a3991962158b645c977dbdf9a5a9db5afd964c3bf1b324efc9ffbbb4 ]] || { echo 'REFUSED: unexpected EFI contents' >&2; exit 3; }
rm -f "$USB/RTX3080-STAGE15-ARM.ONCE"
cp "$backup" "$USB/EFI/BOOT/BOOTX64.EFI"
sync
echo 'PASS: Stage14 restored and Stage15 arm marker removed'
