#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
USB=${1:-}
[[ -n "$USB" && -d "$USB/EFI/BOOT" ]] || { echo 'Usage: remove-child.sh /mnt/f' >&2;exit 2; }
CHILD="$USB/EFI/BOOT/RTX3080-UNLOCK.EFI"
if [[ -f "$CHILD" ]]; then
  ACTUAL=$(sha256sum "$CHILD" | cut -d' ' -f1)
  [[ "$ACTUAL" == 5fee261b5b54e93c930655608cc9356c4f6a0bfc9af457a019166d2c765041ac ]] || { echo "REFUSED: unrecognized child ($ACTUAL)" >&2;exit 3; }
  rm -f "$CHILD"
fi
MARKER="$USB/RTX3080-STAGE15-ARM.ONCE"
if [[ -f "$MARKER" ]];then
  [[ $(cat "$MARKER") == I_ACCEPT_GPU_HANG_RISK ]] || { echo 'REFUSED: unrecognized marker' >&2;exit 4; }
  rm -f "$MARKER"
fi
sync
echo 'Stage16 test child and unused one-shot marker removed; Stage15 bridge preserved.'
