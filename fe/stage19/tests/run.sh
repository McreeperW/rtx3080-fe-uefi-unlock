#!/usr/bin/env bash
set -euo pipefail
D=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
HENGINE=d8da3d26ccd09b37148b20a60ca98fcce6c66eb8c917e56701de4e7561b35ee3
HBRIDGE=e34cc1093d3caaacbe626be19039b6148880f5a40120e51041c59c1971e41e42
[[ "$(sha256sum "$D/out/RTX3080-STAGE19-ENGINE.EFI" | cut -d' ' -f1)" == "$HENGINE" ]]
[[ "$(sha256sum "$D/out/RTX3080-UNLOCK.EFI" | cut -d' ' -f1)" == "$HBRIDGE" ]]
file "$D/out/RTX3080-STAGE19-ENGINE.EFI" "$D/out/RTX3080-UNLOCK.EFI" | grep -c 'PE32+ executable for EFI (application), x86-64' | grep -qx '2'
grep -Fq 'gpu_unlock_610_unlock(&device,&shared)' "$D/engine/app.c"
grep -Fq 'stage19_result=PASS_UNLOCK_SELECTORS_AND_RECONNECT' "$D/src/bridge.c"
grep -Fq 'DisconnectController(gpu,NULL,NULL)' "$D/src/bridge.c"
grep -Fq 'ConnectController(gpu,NULL,NULL,TRUE)' "$D/src/bridge.c"
grep -Fq 'GuSafeToReturn()' "$D/engine/app.c"
grep -Fq 'GPU state changed before failure; Windows handoff refused' "$D/engine/app.c"
grep -Fq 'load_stage19_engine' "$D/src/bridge.c"
grep -Fq 'I_ACCEPT_GPU_HANG_RISK' "$D/install-once.sh"
grep -Fq 'GU_FE_HEADLESS' "$D/build-engine.sh"
# A missing firmware asset must refuse installation before modifying USB or arm marker.
T=$(mktemp -d);trap 'rm -rf "$T"' EXIT
mkdir -p "$T/EFI/BOOT" "$T/RTX3080-UNLOCK/ASSETS"
if [[ -f "$D/../stage15/out/BOOTX64.EFI" && -f "$D/../stage18/out/RTX3080-UNLOCK.EFI" ]];then
 cp "$D/../stage15/out/BOOTX64.EFI" "$T/EFI/BOOT/BOOTX64.EFI"
 cp "$D/../stage18/out/RTX3080-UNLOCK.EFI" "$T/EFI/BOOT/RTX3080-UNLOCK.EFI"
 if bash "$D/install-once.sh" "$T" >/dev/null 2>&1; then echo 'FAIL: assets missing but installed' >&2; exit 3;fi
 [[ ! -e "$T/RTX3080-STAGE15-ARM.ONCE" && ! -e "$T/EFI/BOOT/RTX3080-STAGE19-ENGINE.EFI" ]]
fi
echo 'PASS: Stage19 PE binaries, engine wiring, Stage15 one-shot requirement, and fail-closed installer (if fixtures available)'
