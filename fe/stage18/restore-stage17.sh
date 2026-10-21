#!/usr/bin/env bash
set -euo pipefail
USB=${1:-}
[[ -n "$USB" && -d "$USB/EFI/BOOT" ]] || exit 2
CHILD="$USB/EFI/BOOT/RTX3080-UNLOCK.EFI"
BACKUP="$USB/EFI/BOOT/RTX3080-STAGE17-ARCHIVE.EFI"
MARKER="$USB/RTX3080-STAGE15-ARM.ONCE"
sha(){ sha256sum "$1" | cut -d' ' -f1; }
[[ -f "$BACKUP" && $(sha "$BACKUP") == 1870c3fd372a86bd7f272339abff22d1fd81ac541b8dec0042766aea7ccde021 ]] || { echo 'ERROR: verified Stage17 backup missing' >&2; exit 3;}
[[ -f "$CHILD" && $(sha "$CHILD") == 4be34c5acdff24113a3cfab14e00f7b5438591d0392af9238aacc924da46342b ]] || { echo 'ERROR: cannot overwrite unknown child' >&2;exit 4;}
cp "$BACKUP" "$CHILD"
[[ $(sha "$CHILD") == 1870c3fd372a86bd7f272339abff22d1fd81ac541b8dec0042766aea7ccde021 ]] || exit 5
rm -f "$MARKER"
sync
echo 'PASS: Stage17 child restored, one-shot marker cleared.'
