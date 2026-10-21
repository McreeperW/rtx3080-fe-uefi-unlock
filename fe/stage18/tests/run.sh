#!/usr/bin/env bash
set -euo pipefail
R=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
ROOT=$(cd "$R/../.." && pwd)
[[ $(sha256sum "$R/out/RTX3080-UNLOCK.EFI" | cut -d' ' -f1) == 4be34c5acdff24113a3cfab14e00f7b5438591d0392af9238aacc924da46342b ]] || exit 1
file "$R/out/RTX3080-UNLOCK.EFI" | grep -Eq 'PE32\+.*EFI.*x86-64'
for forbidden in 'Pci.Write(' 'Mem.Write(' 'GuWrite32' 'GuConfigWrite';do
  ! grep -Fq "$forbidden" "$R/src/snapshot.c"
done
grep -q 'DisconnectController(gpu,NULL,NULL)' "$R/src/snapshot.c"
grep -q 'ConnectController(gpu,NULL,NULL,TRUE)' "$R/src/snapshot.c"
grep -q 'inspect_pci_state(gpu,"stage18_released_")' "$R/src/snapshot.c"
[[ -f "$ROOT/fe/stage15/out/BOOTX64.EFI" && -f "$ROOT/fe/stage17/out/RTX3080-UNLOCK.EFI" ]] || exit 2
TMP=$(mktemp -d);trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/usb/EFI/BOOT"
cp "$ROOT/fe/stage15/out/BOOTX64.EFI" "$TMP/usb/EFI/BOOT/BOOTX64.EFI"
cp "$ROOT/fe/stage17/out/RTX3080-UNLOCK.EFI" "$TMP/usb/EFI/BOOT/RTX3080-UNLOCK.EFI"
bash "$R/install-once.sh" "$TMP/usb" >/dev/null
[[ -f "$TMP/usb/RTX3080-STAGE15-ARM.ONCE" ]]
[[ $(sha256sum "$TMP/usb/EFI/BOOT/RTX3080-UNLOCK.EFI" | cut -d' ' -f1) == 4be34c5acdff24113a3cfab14e00f7b5438591d0392af9238aacc924da46342b ]]
if bash "$R/install-once.sh" "$TMP/usb" >/dev/null 2>&1;then echo 'FAIL duplicate install';exit 3;fi
bash "$R/restore-stage17.sh" "$TMP/usb" >/dev/null
[[ $(sha256sum "$TMP/usb/EFI/BOOT/RTX3080-UNLOCK.EFI" | cut -d' ' -f1) == 1870c3fd372a86bd7f272339abff22d1fd81ac541b8dec0042766aea7ccde021 ]]
[[ ! -e "$TMP/usb/RTX3080-STAGE15-ARM.ONCE" ]]
echo 'PASS: Stage18 EFI, read-only PCI/MMIO snapshot, one-shot installation/restore mock'
