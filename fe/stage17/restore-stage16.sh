#!/usr/bin/env bash
set -euo pipefail
USB=${1:-}
[[ -n "$USB" && -d "$USB/EFI/BOOT" ]] || { echo 'Usage: restore-stage16.sh /mnt/f' >&2;exit 2; }
CHILD="$USB/EFI/BOOT/RTX3080-UNLOCK.EFI"
BACKUP="$USB/EFI/BOOT/RTX3080-STAGE16-ARCHIVE.EFI"
MARKER="$USB/RTX3080-STAGE15-ARM.ONCE"
gethash(){ sha256sum "$1" | cut -d' ' -f1; }
H16=5fee261b5b54e93c930655608cc9356c4f6a0bfc9af457a019166d2c765041ac
H17=1870c3fd372a86bd7f272339abff22d1fd81ac541b8dec0042766aea7ccde021
[[ -f "$BACKUP" && $(gethash "$BACKUP") == "$H16" ]] || { echo 'ERROR: known Stage16 backup missing or changed' >&2;exit 3; }
[[ -f "$CHILD" && $(gethash "$CHILD") == "$H17" ]] || { echo 'ERROR: child is not exact Stage17 binary' >&2;exit 4; }
# Remove marker even if it was not consumed, so the next boot cannot repeat Stage17.
rm -f "$MARKER"
cp "$BACKUP" "$CHILD"
[[ $(gethash "$CHILD") == "$H16" ]] || { echo 'ERROR: Stage16 restore failed' >&2;exit 5; }
sync
echo 'PASS: Stage16 diagnostic child restored; Stage17 no longer armed.'
