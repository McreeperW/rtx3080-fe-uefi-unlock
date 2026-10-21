#!/usr/bin/env bash
# Read-only GPU probe + existing Windows Boot Manager EFI chainloader.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
USB="${1%/}"
if [[ ! "$USB" =~ ^/mnt/[d-z]$ || ! -d "$USB/EFI/BOOT" || ! -d "$USB/RTX3080-UNLOCK/ASSETS" ]]; then
  echo 'Refusing: expected the Stage 12 removable USB root, e.g. /mnt/f, with EFI/BOOT and RTX3080-UNLOCK/ASSETS.' >&2
  exit 2
fi
EFI="$ROOT/out/BOOTX64.EFI"
[[ -s "$EFI" ]] || { echo 'Missing Stage 13 EFI binary' >&2; exit 3; }
( cd "$ROOT" && sha256sum -c SHA256SUMS.txt )
mkdir -p "$USB/RTX3080-UNLOCK/BACKUP"
BACKUP="$USB/RTX3080-UNLOCK/BACKUP/BOOTX64.STAGE12.EFI"
if [[ ! -e "$BACKUP" ]]; then
  cp "$USB/EFI/BOOT/BOOTX64.EFI" "$BACKUP"
  echo 'Backed up existing Stage 12 EFI to RTX3080-UNLOCK/BACKUP/'
else
  echo 'Existing backup preserved (not overwritten).'
fi
cp "$EFI" "$USB/EFI/BOOT/BOOTX64.EFI"
sync
cmp -s "$EFI" "$USB/EFI/BOOT/BOOTX64.EFI" || { echo 'USB EFI verify failed!' >&2; exit 4; }
echo 'Installed Stage 13 read-only bootbridge on the USB. GPU unlock is NOT included.'
sha256sum "$USB/EFI/BOOT/BOOTX64.EFI"
echo 'Windows EFI, NVRAM BootOrder and the internal disk were not modified.'
