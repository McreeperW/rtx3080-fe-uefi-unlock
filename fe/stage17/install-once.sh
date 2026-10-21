#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
USB=${1:-}
BRIDGE_HASH=e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00
STAGE16_HASH=5fee261b5b54e93c930655608cc9356c4f6a0bfc9af457a019166d2c765041ac
STAGE17_HASH=1870c3fd372a86bd7f272339abff22d1fd81ac541b8dec0042766aea7ccde021
[[ -n "$USB" && -d "$USB/EFI/BOOT" ]] || { echo 'ERROR: USB/EFI/BOOT missing (mount verified USB first)' >&2;exit 2; }
BRIDGE="$USB/EFI/BOOT/BOOTX64.EFI"
CHILD="$USB/EFI/BOOT/RTX3080-UNLOCK.EFI"
BACKUP="$USB/EFI/BOOT/RTX3080-STAGE16-ARCHIVE.EFI"
MARKER="$USB/RTX3080-STAGE15-ARM.ONCE"
gethash(){ sha256sum "$1" | cut -d' ' -f1; }
[[ -f "$BRIDGE" && $(gethash "$BRIDGE") == "$BRIDGE_HASH" ]] || { echo 'ERROR: exact proven Stage15 bridge is missing' >&2;exit 3; }
[[ -f "$ROOT/out/RTX3080-UNLOCK.EFI" && $(gethash "$ROOT/out/RTX3080-UNLOCK.EFI") == "$STAGE17_HASH" ]] || { echo 'ERROR: Stage17 child hash mismatch' >&2;exit 4; }
[[ ! -e "$MARKER" ]] || { echo 'ERROR: existing one-shot marker; will not overwrite' >&2;exit 5; }
[[ ! -e "$BACKUP" ]] || { echo 'ERROR: Stage16 archive already exists; refusing overwrite' >&2;exit 6; }
[[ -f "$CHILD" && $(gethash "$CHILD") == "$STAGE16_HASH" ]] || { echo 'ERROR: expected Stage16 read-only child (do not overwrite an unknown EFI)' >&2;exit 7; }
# No -p: WSL drvfs FAT32 can reject preservation of timestamps.
cp "$CHILD" "$BACKUP"
[[ $(gethash "$BACKUP") == "$STAGE16_HASH" ]] || { echo 'ERROR: Stage16 archive failed integrity check' >&2;exit 8; }
cp "$ROOT/out/RTX3080-UNLOCK.EFI" "$CHILD"
if [[ $(gethash "$CHILD") != "$STAGE17_HASH" ]]; then
  cp "$BACKUP" "$CHILD"
  echo 'ERROR: Stage17 USB copy verification failed, restored Stage16' >&2;exit 9
fi
printf 'I_ACCEPT_GPU_HANG_RISK' > "$MARKER"
sync
if [[ ! -f "$MARKER" || $(cat "$MARKER") != 'I_ACCEPT_GPU_HANG_RISK' ]]; then
  rm -f "$MARKER";cp "$BACKUP" "$CHILD"
  echo 'ERROR: one-shot marker did not persist, restored Stage16' >&2;exit 10
fi
echo 'PASS: Stage17 ONE-SHOT controller disconnect/reconnect test installed.'
echo 'NOTICE: NVIDIA GPU screen may go black; hang is possible. This does NOT unlock GPU.'
echo 'Boot UEFI USB once, then inspect RTX3080-STAGE17-RECONNECT.TXT and new RTX3080-STAGE15-*.TXT.'
