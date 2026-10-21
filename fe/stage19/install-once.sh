#!/usr/bin/env bash
set -euo pipefail
D=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
USB=${1:-}
[[ -n "$USB" && -d "$USB/EFI/BOOT" ]] || { echo 'ERROR: usage install-once.sh /mnt/f (USB only)' >&2; exit 2; }
sha(){ sha256sum "$1" | cut -d' ' -f1; }
H15=e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00
H18=4be34c5acdff24113a3cfab14e00f7b5438591d0392af9238aacc924da46342b
H19ENGINE=d8da3d26ccd09b37148b20a60ca98fcce6c66eb8c917e56701de4e7561b35ee3
H19BRIDGE=e34cc1093d3caaacbe626be19039b6148880f5a40120e51041c59c1971e41e42
BRIDGE="$USB/EFI/BOOT/BOOTX64.EFI"
CHILD="$USB/EFI/BOOT/RTX3080-UNLOCK.EFI"
ENGINE="$USB/EFI/BOOT/RTX3080-STAGE19-ENGINE.EFI"
BAK="$USB/EFI/BOOT/RTX3080-STAGE18-STAGE19-BACKUP.EFI"
ARM="$USB/RTX3080-STAGE15-ARM.ONCE"
[[ -f "$BRIDGE" && "$(sha "$BRIDGE")" == "$H15" ]] || { echo 'ERROR: proven Stage15 USB boot bridge not installed' >&2;exit 3; }
[[ -f "$CHILD" && "$(sha "$CHILD")" == "$H18" ]] || { echo 'ERROR: expected Stage18 diagnostic child not installed' >&2;exit 4; }
[[ "$(sha "$D/out/RTX3080-STAGE19-ENGINE.EFI")" == "$H19ENGINE" && "$(sha "$D/out/RTX3080-UNLOCK.EFI")" == "$H19BRIDGE" ]] || { echo 'ERROR: Stage19 compiled binaries mismatch' >&2;exit 5; }
[[ ! -e "$ARM" && ! -e "$BAK" && ! -e "$ENGINE" ]] || { echo 'ERROR: already armed or existing backup/engine; refusing overwrite' >&2;exit 6; }
# The user's previously successful Stage12 run used exactly these five pinned assets.
ASSETS="$USB/RTX3080-UNLOCK/ASSETS"
[[ -d "$ASSETS" ]] || { echo "ERROR: firmware ASSETS folder missing: $ASSETS" >&2;exit 7; }
files=(booter_load_ga102.bin v67-ga102-610.43.03.bin gsp_ga10x.bin gsp_boot_ga102.bin gsp_boot_desc_ga102.bin)
sizes=(61256 64000 84277400 24576 84)
hashes=(056cbc064a888f80717726c54d7f64fcd68044d123870c95a74f37f4975dcaa4 2a0aa1f86d0b6afff5214d641d87d46dc98ef06ed908ff05d0ddbad5ffb7e197 572373620a37418f24dc16b5031c39338778c3257e48e8408de9a57291b24f3a 3df651acb174fd5f5561a1996965b192f50c5d763168446ed0d4ead19d895768 cc5a3fc1f4c47f2fe107b1b68f0176d8a448e6dec84b575e5560ccef5262bf15)
for i in "${!files[@]}"; do
    f="$ASSETS/${files[i]}"
    [[ -f "$f" ]] || { echo "ERROR: asset missing: $f" >&2; exit 8; }
    bytes=$(stat -c %s "$f")
    [[ "$bytes" == "${sizes[i]}" && "$(sha "$f")" == "${hashes[i]}" ]] || { echo "ERROR: asset size/hash mismatch: $f" >&2; exit 9; }
    echo "PASS: ${files[i]} hash"
done
# Preflight is read-only until both images are verified and firmware assets match.
cp "$CHILD" "$BAK"
[[ "$(sha "$BAK")" == "$H18" ]] || { echo 'ERROR: failed to back up Stage18' >&2;exit 10; }
trap 'if [[ -e "$BAK" && "$(sha "$BAK")" == "$H18" ]]; then cp "$BAK" "$CHILD"; fi; rm -f "$ARM" "$ENGINE"; sync; echo "Rollback attempted after installation error" >&2' ERR
cp "$D/out/RTX3080-STAGE19-ENGINE.EFI" "$ENGINE"
cp "$D/out/RTX3080-UNLOCK.EFI" "$CHILD"
[[ "$(sha "$ENGINE")" == "$H19ENGINE" && "$(sha "$CHILD")" == "$H19BRIDGE" ]] || { echo 'ERROR: copy checksum mismatch' >&2;exit 11; }
printf 'I_ACCEPT_GPU_HANG_RISK' > "$ARM"
sync
[[ "$(cat "$ARM")" == I_ACCEPT_GPU_HANG_RISK ]] || { echo 'ERROR: one-shot arm failed' >&2;exit 12; }
trap - ERR
echo 'PASS: Stage19 EXPERIMENTAL unlock armed ONCE.'
echo 'WARNING: Real GPU DMA/FWSEC/SEC2/FLR. A firmware hang CAN prevent Windows and error logs.'
