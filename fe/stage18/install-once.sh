#!/usr/bin/env bash
set -euo pipefail
R=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
USB=${1:-}
[[ -n "$USB" && -d "$USB/EFI/BOOT" ]] || { echo 'ERROR: USB/EFI/BOOT missing' >&2;exit 2; }
BRIDGE="$USB/EFI/BOOT/BOOTX64.EFI"
CHILD="$USB/EFI/BOOT/RTX3080-UNLOCK.EFI"
BACKUP="$USB/EFI/BOOT/RTX3080-STAGE17-ARCHIVE.EFI"
MARKER="$USB/RTX3080-STAGE15-ARM.ONCE"
sha(){ sha256sum "$1" | cut -d' ' -f1; }
[[ -f "$BRIDGE" && $(sha "$BRIDGE") == e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00 ]] || { echo 'ERROR: proven Stage15 boot bridge missing' >&2;exit 3; }
[[ -f "$CHILD" && $(sha "$CHILD") == 1870c3fd372a86bd7f272339abff22d1fd81ac541b8dec0042766aea7ccde021 ]] || { echo 'ERROR: current USB child must be exact Stage17 release/reconnect image' >&2;exit 4; }
[[ -f "$R/out/RTX3080-UNLOCK.EFI" && $(sha "$R/out/RTX3080-UNLOCK.EFI") == 4be34c5acdff24113a3cfab14e00f7b5438591d0392af9238aacc924da46342b ]] || { echo 'ERROR: Stage18 image hash mismatch' >&2;exit 5; }
[[ ! -e "$BACKUP" ]] || { echo 'ERROR: backup exists - no overwrite' >&2;exit 6; }
[[ ! -e "$MARKER" ]] || { echo 'ERROR: one-shot marker already exists' >&2;exit 7; }
cp "$CHILD" "$BACKUP"
[[ $(sha "$BACKUP") == 1870c3fd372a86bd7f272339abff22d1fd81ac541b8dec0042766aea7ccde021 ]] || { echo 'ERROR: backup mismatch' >&2;exit 8; }
cp "$R/out/RTX3080-UNLOCK.EFI" "$CHILD"
if [[ $(sha "$CHILD") != 4be34c5acdff24113a3cfab14e00f7b5438591d0392af9238aacc924da46342b ]];then cp "$BACKUP" "$CHILD";echo 'ERROR: copied image hash mismatch' >&2;exit 9;fi
printf 'I_ACCEPT_GPU_HANG_RISK' > "$MARKER"
sync
if [[ ! -f "$MARKER" || $(cat "$MARKER") != I_ACCEPT_GPU_HANG_RISK ]];then rm -f "$MARKER";cp "$BACKUP" "$CHILD";echo 'ERROR: failed to write marker' >&2;exit 10;fi
echo 'PASS: Stage18 snapshot test armed for ONE UEFI USB boot.'
echo 'No manual PCI/MMIO writes, unlock, DMA or FLR. Driver disconnect/reconnect still carries hang risk.'
