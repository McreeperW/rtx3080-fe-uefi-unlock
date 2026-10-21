#!/usr/bin/env bash
set -euo pipefail
USB=${1:-}
[[ -n "$USB" && -d "$USB/EFI/BOOT" ]] || { echo 'Usage: restore-stage18.sh /mnt/f' >&2;exit 2; }
BAK="$USB/EFI/BOOT/RTX3080-STAGE18-STAGE19-BACKUP.EFI"
CHILD="$USB/EFI/BOOT/RTX3080-UNLOCK.EFI"
EXPECTED=4be34c5acdff24113a3cfab14e00f7b5438591d0392af9238aacc924da46342b
[[ -f "$BAK" && "$(sha256sum "$BAK" | cut -d' ' -f1)" == "$EXPECTED" ]] || { echo 'ERROR: backup missing/invalid' >&2;exit 3; }
cp "$BAK" "$CHILD"
[[ "$(sha256sum "$CHILD" | cut -d' ' -f1)" == "$EXPECTED" ]] || { echo 'ERROR: restored child mismatch' >&2;exit 4; }
rm -f "$USB/RTX3080-STAGE15-ARM.ONCE"
sync
echo 'PASS: previous Stage18 image restored. Stage15 bridge untouched.'
