#!/usr/bin/env bash
# Disable persistent execution. Do not erase AUTO.PENDING (failure evidence).
set -euo pipefail
USB="${1:-}"
if [[ -z "$USB" || "$USB" == '/' || ! -d "$USB/EFI/BOOT" ]]; then
 echo 'Usage: bash scripts/disable-auto-usb.sh /mnt/f' >&2; exit 2
fi
rm -f -- "$USB/RTX3080-STAGE15-AUTO.ON"
sync
echo 'Disabled persistent auto execution. Existing AUTO.PENDING (if any) retained.'
echo 'To bypass even the bridge, simply remove the USB and boot normally.'
