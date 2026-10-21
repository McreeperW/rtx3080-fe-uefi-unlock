#!/usr/bin/env bash
set -euo pipefail
USB=${1:-}
[[ -n "$USB" && -d "$USB/EFI/BOOT" ]] || { echo 'ERROR: supply /mnt/f' >&2;exit 2; }
BASE="$USB/EFI/BOOT"
BACK="$BASE/RTX3080-STAGE18-STAGE19-BACKUP.EFI"
CHILD="$BASE/RTX3080-UNLOCK.EFI"
HASH=4be34c5acdff24113a3cfab14e00f7b5438591d0392af9238aacc924da46342b
[[ -f "$BACK" && "$(sha256sum "$BACK" | cut -d' ' -f1)" == "$HASH" ]] || { echo 'ERROR: trusted Stage18 backup missing/mismatch' >&2;exit 3; }
cp "$BACK" "$CHILD"
[[ "$(sha256sum "$CHILD" | cut -d' ' -f1)" == "$HASH" ]] || { echo 'ERROR: restore hash mismatch' >&2;exit 4; }
rm -f "$USB/RTX3080-STAGE15-ARM.ONCE"
sync
echo 'PASS: Stage18 EFI restored, one-shot marker removed. Stage15 unchanged.'
