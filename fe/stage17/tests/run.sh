#!/usr/bin/env bash
set -euo pipefail
R=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
ROOT=$(cd "$R/../.." && pwd)
[[ $(sha256sum "$R/out/RTX3080-UNLOCK.EFI" | cut -d' ' -f1) == 1870c3fd372a86bd7f272339abff22d1fd81ac541b8dec0042766aea7ccde021 ]] || exit 1
file "$R/out/RTX3080-UNLOCK.EFI" | grep -E 'PE32\+.*EFI.*x86-64' >/dev/null
grep -q 'DisconnectController(gpu,NULL,NULL)' "$R/src/reconnect.c"
grep -q 'ConnectController(gpu,NULL,NULL,TRUE)' "$R/src/reconnect.c"
for bad in 'Pci.Write(' 'Mem.Write(' 'FLR' ; do
  case "$bad" in FLR) continue;; esac
  ! grep -Fq "$bad" "$R/src/reconnect.c"
done
[[ -f "$ROOT/fe/stage15/out/BOOTX64.EFI" ]] || { echo 'Stage15 bridge missing from repo' >&2;exit 2; }
[[ -f "$ROOT/fe/stage16/out/RTX3080-UNLOCK.EFI" ]] || { echo 'Stage16 child missing from repo' >&2;exit 2; }
TMP=$(mktemp -d);trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/usb/EFI/BOOT"
cp "$ROOT/fe/stage15/out/BOOTX64.EFI" "$TMP/usb/EFI/BOOT/BOOTX64.EFI"
cp "$ROOT/fe/stage16/out/RTX3080-UNLOCK.EFI" "$TMP/usb/EFI/BOOT/RTX3080-UNLOCK.EFI"
bash "$R/install-once.sh" "$TMP/usb" >/dev/null
[[ -f "$TMP/usb/RTX3080-STAGE15-ARM.ONCE" ]]
[[ $(sha256sum "$TMP/usb/EFI/BOOT/RTX3080-UNLOCK.EFI" | cut -d' ' -f1) == 1870c3fd372a86bd7f272339abff22d1fd81ac541b8dec0042766aea7ccde021 ]]
if bash "$R/install-once.sh" "$TMP/usb" >/dev/null 2>&1;then echo 'FAIL: allowed duplicate installation';exit 3;fi
bash "$R/restore-stage16.sh" "$TMP/usb" >/dev/null
[[ ! -f "$TMP/usb/RTX3080-STAGE15-ARM.ONCE" ]]
echo 'PASS: Stage17 EFI x64, controller code and installation/restore mock tests'
