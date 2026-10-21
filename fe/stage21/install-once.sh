#!/usr/bin/env bash
set -euo pipefail
D=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
USB=${1:-}
[[ -n "$USB" && -d "$USB/EFI/BOOT" ]] || { echo 'ERROR: need mounted FAT32 USB directory, e.g. /mnt/f'; exit 2; }
sha(){ sha256sum "$1" | cut -d' ' -f1; }
H15=e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00
H18=4be34c5acdff24113a3cfab14e00f7b5438591d0392af9238aacc924da46342b
H19=d8da3d26ccd09b37148b20a60ca98fcce6c66eb8c917e56701de4e7561b35ee3
H20=8ef922e0804bf5b0d68f028e8af2c77b676ebfb9b0b3307b7311516e8747d4c5
H21=0d578a0612fb8a59b7320a0e038d2f19b03f9573b2a995f10ed44e6e10b32373
B="$USB/EFI/BOOT"
BOOT="$B/BOOTX64.EFI"
BRIDGE="$B/RTX3080-UNLOCK.EFI"
ENGINE="$B/RTX3080-STAGE19-ENGINE.EFI"
BACKUP="$B/RTX3080-STAGE21-ENGINE-PREVIOUS.EFI"
STAGE18="$B/RTX3080-STAGE18-STAGE19-BACKUP.EFI"
ARM="$USB/RTX3080-STAGE15-ARM.ONCE"
NEW="$D/out/RTX3080-STAGE19-ENGINE.EFI"
[[ -f "$BOOT" && "$(sha "$BOOT")" == "$H15" ]] || { echo 'ERROR: Stage15 BOOTX64.EFI mismatch'; exit 3; }
[[ -f "$BRIDGE" && "$(sha "$BRIDGE")" == "$H20" ]] || { echo 'ERROR: Stage20 bridge missing or mismatched'; exit 4; }
[[ -f "$ENGINE" && "$(sha "$ENGINE")" == "$H19" ]] || { echo 'ERROR: Stage19 engine not present (do not overwrite unexpected engine)'; exit 5; }
[[ -f "$STAGE18" && "$(sha "$STAGE18")" == "$H18" ]] || { echo 'ERROR: Stage18 backup mismatch'; exit 6; }
[[ -f "$NEW" && "$(sha "$NEW")" == "$H21" ]] || { echo 'ERROR: Stage21 engine mismatch'; exit 7; }
[[ ! -e "$BACKUP" && ! -e "$ARM" ]] || { echo 'ERROR: Stage21 backup or active one-shot marker already present'; exit 8; }
ASSETS="$USB/RTX3080-UNLOCK/ASSETS"
[[ -d "$ASSETS" ]] || { echo 'ERROR: firmware ASSETS missing'; exit 9; }
files=(booter_load_ga102.bin v67-ga102-610.43.03.bin gsp_ga10x.bin gsp_boot_ga102.bin gsp_boot_desc_ga102.bin)
sizes=(61256 64000 84277400 24576 84)
hashes=(056cbc064a888f80717726c54d7f64fcd68044d123870c95a74f37f4975dcaa4 2a0aa1f86d0b6afff5214d641d87d46dc98ef06ed908ff05d0ddbad5ffb7e197 572373620a37418f24dc16b5031c39338778c3257e48e8408de9a57291b24f3a 3df651acb174fd5f5561a1996965b192f50c5d763168446ed0d4ead19d895768 cc5a3fc1f4c47f2fe107b1b68f0176d8a448e6dec84b575e5560ccef5262bf15)
for i in "${!files[@]}";do
  f="$ASSETS/${files[i]}"
  [[ -f "$f" && "$(stat -c %s "$f")" == "${sizes[i]}" && "$(sha "$f")" == "${hashes[i]}" ]] || { echo "ERROR: asset mismatch: ${files[i]}"; exit 10; }
done
cp "$ENGINE" "$BACKUP"
[[ "$(sha "$BACKUP")" == "$H19" ]] || { echo 'ERROR: backup verification failure'; exit 11; }
COMMITTED=0
rollback(){
  if [[ "$COMMITTED" != 1 ]];then
    if [[ -f "$BACKUP" && "$(sha "$BACKUP")" == "$H19" ]];then cp "$BACKUP" "$ENGINE" || :;fi
    rm -f "$ARM"
    sync
    echo 'ERROR: Stage21 install failed. Engine restored where possible.' >&2
  fi
}
trap rollback EXIT
cp "$NEW" "$ENGINE"
[[ "$(sha "$ENGINE")" == "$H21" ]] || { echo 'ERROR: copied engine hash mismatch'; exit 12; }
printf 'I_ACCEPT_GPU_HANG_RISK' > "$ARM"
sync
[[ "$(cat "$ARM")" == 'I_ACCEPT_GPU_HANG_RISK' ]] || { echo 'ERROR: one-shot marker write failed'; exit 13; }
COMMITTED=1
trap - EXIT
echo 'PASS: Stage21 guarded cold-state engine installed and armed ONCE.'
echo 'WARNING: this is a real GPU firmware transaction test; hang and forced shutdown remain possible.'
