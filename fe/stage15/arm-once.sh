#!/usr/bin/env bash
set -euo pipefail
USB="${1:-}"
[[ -n "$USB" && -d "$USB/EFI/BOOT" ]] || { echo 'Usage: arm-once.sh /mnt/f' >&2; exit 2; }
[[ -f "$USB/EFI/BOOT/RTX3080-UNLOCK.EFI" ]] || { echo 'REFUSED: no external headless-compatible EFI module found' >&2; exit 3; }
[[ "$(sha256sum "$USB/EFI/BOOT/BOOTX64.EFI" | cut -d' ' -f1)" == e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00 ]] || { echo 'REFUSED: Stage15 USB bridge not installed' >&2; exit 4; }
[[ ! -e "$USB/RTX3080-STAGE15-ARM.ONCE" ]] || { echo 'REFUSED: already armed' >&2; exit 5; }
printf 'I_ACCEPT_GPU_HANG_RISK' > "$USB/RTX3080-STAGE15-ARM.ONCE"
sync
echo 'ARMED ONCE: next Stage15 USB boot loads RTX3080-UNLOCK.EFI. This is NOT proof that module will safely return.'
