#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
USB=${1:-}
[[ -n "$USB" && -d "$USB/EFI/BOOT" ]] || { echo 'Usage: install-once.sh /mnt/f' >&2;exit 2; }
EXPECTED_BRIDGE=e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00
CHILD_SHA=5fee261b5b54e93c930655608cc9356c4f6a0bfc9af457a019166d2c765041ac
BRIDGE="$USB/EFI/BOOT/BOOTX64.EFI"
CHILD="$USB/EFI/BOOT/RTX3080-UNLOCK.EFI"
MARKER="$USB/RTX3080-STAGE15-ARM.ONCE"
[[ -f "$BRIDGE" && $(sha256sum "$BRIDGE" | cut -d' ' -f1) == "$EXPECTED_BRIDGE" ]] || { echo 'REFUSED: exact tested Stage15 bridge not found on USB' >&2; exit 3; }
[[ $(sha256sum "$HERE/out/RTX3080-UNLOCK.EFI" | cut -d' ' -f1) == "$CHILD_SHA" ]] || { echo 'REFUSED: child hash mismatch' >&2; exit 4; }
[[ ! -e "$CHILD" ]] || { echo "REFUSED: child already exists: $CHILD" >&2; exit 5; }
[[ ! -e "$MARKER" ]] || { echo "REFUSED: existing arm marker: $MARKER" >&2; exit 6; }
cp "$HERE/out/RTX3080-UNLOCK.EFI" "$CHILD"
if [[ $(sha256sum "$CHILD" | cut -d' ' -f1) != "$CHILD_SHA" ]];then
  rm -f "$CHILD";echo 'REFUSED: child USB copy mismatch' >&2;exit 7
fi
printf 'I_ACCEPT_GPU_HANG_RISK' > "$MARKER"
sync
[[ -f "$MARKER" && $(cat "$MARKER") == I_ACCEPT_GPU_HANG_RISK ]] || { echo 'REFUSED: marker not saved correctly' >&2; exit 8; }
echo 'PASS: READ-ONLY Stage16 child installed and one-shot marker armed.'
echo 'NEXT USB BOOT: child intentionally returns EFI_ACCESS_DENIED -> Stage15 logs FAILED_CHILD_RETURNED -> Windows handoff.'
echo 'NO UNLOCK, NO GPU REGISTRY WRITES, NO FLR OR FIRMWARE EXECUTION.'
