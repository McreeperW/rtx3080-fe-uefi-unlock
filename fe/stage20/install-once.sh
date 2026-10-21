#!/usr/bin/env bash
set -euo pipefail
D=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
USB=${1:-}
[[ -n "$USB" && -d "$USB/EFI/BOOT" ]] || { echo 'ERROR: provide mounted USB path /mnt/f' >&2; exit 2; }
sha(){ sha256sum "$1" | cut -d' ' -f1; }
H15=e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00
H18=4be34c5acdff24113a3cfab14e00f7b5438591d0392af9238aacc924da46342b
H19BRIDGE=e34cc1093d3caaacbe626be19039b6148880f5a40120e51041c59c1971e41e42
H19ENGINE=d8da3d26ccd09b37148b20a60ca98fcce6c66eb8c917e56701de4e7561b35ee3
H20=8ef922e0804bf5b0d68f028e8af2c77b676ebfb9b0b3307b7311516e8747d4c5
BRIDGE="$USB/EFI/BOOT/BOOTX64.EFI"
CHILD="$USB/EFI/BOOT/RTX3080-UNLOCK.EFI"
ENGINE="$USB/EFI/BOOT/RTX3080-STAGE19-ENGINE.EFI"
STAGE18="$USB/EFI/BOOT/RTX3080-STAGE18-STAGE19-BACKUP.EFI"
BAK="$USB/EFI/BOOT/RTX3080-STAGE20-PREVIOUS.EFI"
ARM="$USB/RTX3080-STAGE15-ARM.ONCE"
[[ -f "$BRIDGE" && "$(sha "$BRIDGE")" == "$H15" ]] || { echo 'ERROR: Stage15 bridge missing or hash mismatched' >&2;exit 3; }
[[ -f "$ENGINE" && "$(sha "$ENGINE")" == "$H19ENGINE" ]] || { echo 'ERROR: Stage19 engine missing or hash mismatched' >&2;exit 4; }
[[ -f "$STAGE18" && "$(sha "$STAGE18")" == "$H18" ]] || { echo 'ERROR: Stage18 backup missing or hash mismatched' >&2;exit 5; }
[[ -f "$CHILD" ]] || { echo 'ERROR: current child missing' >&2;exit 6; }
CURRENT=$(sha "$CHILD")
[[ "$CURRENT" == "$H19BRIDGE" || "$CURRENT" == "$H18" ]] || { echo 'ERROR: current child is neither exact Stage19 nor Stage18 binary' >&2;exit 7; }
[[ "$(sha "$D/out/RTX3080-UNLOCK.EFI")" == "$H20" ]] || { echo 'ERROR: Stage20 EFI binary hash mismatch' >&2;exit 8; }
[[ ! -e "$ARM" && ! -e "$BAK" ]] || { echo 'ERROR: armed marker or previous backup exists. Do not overwrite' >&2;exit 9; }
ASSETS="$USB/RTX3080-UNLOCK/ASSETS"
[[ -d "$ASSETS" ]] || { echo 'ERROR: RTX3080-UNLOCK/ASSETS missing' >&2;exit 10; }
files=(booter_load_ga102.bin v67-ga102-610.43.03.bin gsp_ga10x.bin gsp_boot_ga102.bin gsp_boot_desc_ga102.bin)
sizes=(61256 64000 84277400 24576 84)
hashes=(056cbc064a888f80717726c54d7f64fcd68044d123870c95a74f37f4975dcaa4 2a0aa1f86d0b6afff5214d641d87d46dc98ef06ed908ff05d0ddbad5ffb7e197 572373620a37418f24dc16b5031c39338778c3257e48e8408de9a57291b24f3a 3df651acb174fd5f5561a1996965b192f50c5d763168446ed0d4ead19d895768 cc5a3fc1f4c47f2fe107b1b68f0176d8a448e6dec84b575e5560ccef5262bf15)
for i in "${!files[@]}"; do
  f="$ASSETS/${files[i]}"
  [[ -f "$f" && "$(stat -c %s "$f")" == "${sizes[i]}" && "$(sha "$f")" == "${hashes[i]}" ]] || { echo "ERROR: firmware asset mismatch: ${files[i]}" >&2;exit 11; }
done
cp "$CHILD" "$BAK"
[[ "$(sha "$BAK")" == "$CURRENT" ]] || { echo 'ERROR: backup verification failed' >&2;exit 12; }
COMMITTED=0
rollback(){
  if [[ "$COMMITTED" != 1 ]]; then
    if [[ -f "$BAK" && "$(sha "$BAK")" == "$CURRENT" ]]; then cp "$BAK" "$CHILD"; fi
    rm -f "$ARM";sync
    echo 'ERROR: install aborted; prior EFI restored if possible' >&2
  fi
}
trap rollback EXIT
cp "$D/out/RTX3080-UNLOCK.EFI" "$CHILD"
[[ "$(sha "$CHILD")" == "$H20" ]] || { echo 'ERROR: child copy mismatch' >&2;exit 13; }
printf 'I_ACCEPT_GPU_HANG_RISK' > "$ARM"
sync
[[ "$(cat "$ARM")" == 'I_ACCEPT_GPU_HANG_RISK' ]] || { echo 'ERROR: arm write mismatch' >&2;exit 14; }
COMMITTED=1
trap - EXIT
echo 'PASS: Stage20 SKIP GOP reconnect, armed ONCE.'
echo 'DANGER: Windows may not boot without GOP; hard power-off may be necessary.'
