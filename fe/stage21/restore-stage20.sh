#!/usr/bin/env bash
set -euo pipefail
USB=${1:-}
[[ -n "$USB" && -d "$USB/EFI/BOOT" ]] || { echo 'ERROR: need mounted USB path'; exit 2; }
sha(){ sha256sum "$1" | cut -d' ' -f1; }
H15=e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00
H20=8ef922e0804bf5b0d68f028e8af2c77b676ebfb9b0b3307b7311516e8747d4c5
H19=d8da3d26ccd09b37148b20a60ca98fcce6c66eb8c917e56701de4e7561b35ee3
B="$USB/EFI/BOOT"
PREV="$B/RTX3080-STAGE21-ENGINE-PREVIOUS.EFI"
ENGINE="$B/RTX3080-STAGE19-ENGINE.EFI"
[[ -f "$PREV" && "$(sha "$PREV")" == "$H19" ]] || { echo 'ERROR: original engine backup is missing'; exit 3; }
[[ -f "$B/BOOTX64.EFI" && "$(sha "$B/BOOTX64.EFI")" == "$H15" ]] || { echo 'ERROR: BOOTX64 unexpected'; exit 4; }
[[ -f "$B/RTX3080-UNLOCK.EFI" && "$(sha "$B/RTX3080-UNLOCK.EFI")" == "$H20" ]] || { echo 'ERROR: bridge unexpected'; exit 5; }
cp "$PREV" "$ENGINE"
[[ "$(sha "$ENGINE")" == "$H19" ]] || { echo 'ERROR: original engine restore hash mismatch'; exit 6; }
rm -f "$USB/RTX3080-STAGE15-ARM.ONCE"
sync
echo 'PASS: Original Stage19 engine restored; Stage20 bridge retained; one-shot marker removed.'
