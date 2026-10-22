#!/usr/bin/env bash
# Independently builds only fe/stage26/engine/; never invokes Stage21 builder.
# Builds an EFI with Stage20-compatible filename and a labeled archive copy.
# No USB writes, GPU register writes, or firmware execution.
set -euo pipefail
D=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
BASE=${PROJECT_ROOT:-"$(cd "$D/../.." && pwd)"}
EDK="$BASE/deps/edk2/MdePkg/Include"
[[ -f "$EDK/Uefi.h" ]] || { echo 'ERROR: EDK2 headers missing' >&2; exit 2; }
for tool in clang lld-link file sha256sum python3; do
  command -v "$tool" >/dev/null || { echo "ERROR: missing $tool" >&2; exit 3; }
done
APP="$D/engine/app.c"; COLD="$D/engine/fe_cold.h"
[[ -f "$APP" && -f "$COLD" ]] || { echo 'ERROR: independent Stage26 sources missing' >&2; exit 4; }
grep -Fq 'STAGE26 SS0 snapshot' "$APP" &&
grep -Fq 'a.environment_ok_without_ss0?' "$APP" &&
grep -Fq 's->ss0!=0xffffffffU' "$COLD" || {
  echo 'ERROR: Stage26 log-only policy not detected' >&2; exit 5;
}
# Refuse to build Stage26 if Stage21 source was replaced by the Stage26 policy.
[[ -f "$BASE/fe/stage21/engine/app.c" ]] &&
grep -Fq 'a.known_observation && a.gfw_idle && a.signature_slots_supported?' "$BASE/fe/stage21/engine/app.c" || {
  echo 'ERROR: independent Stage21 strict-policy source missing' >&2; exit 6;
}
mkdir -p "$D/out/objects"
sources=(app app_io ownership format runtime sha256 checks port compat vendor/unlock_610 vendor/prep_610 vendor/layout_610 vendor/vbios_610 vendor/gfw_eval)
objects=()
for s in "${sources[@]}"; do
  path="$D/engine/${s}.c"
  [[ -f "$path" ]] || { echo "ERROR: Stage26 missing source: $path" >&2; exit 7; }
  obj="$D/out/objects/${s//\//_}.obj"
  clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -Wno-unused-parameter -D__KERNEL__ -DGU_UEFI_PORT -DGU_FE_EXPERIMENTAL -DGU_FE_HEADLESS -DGU_TARGET_PCI_ID=0x220610deU -I "$D/engine" -I "$D/engine/compat" -I "$EDK" -I "$EDK/X64" -c "$path" -o "$obj"
  objects+=("$obj")
done
BRIDGE="$D/out/RTX3080-STAGE19-ENGINE.EFI"
ARCHIVE="$D/out/RTX3080-STAGE26-ENGINE.EFI"
lld-link /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf /out:"$BRIDGE" "${objects[@]}"
file "$BRIDGE" | grep -q 'PE32+ executable for EFI (application), x86-64'
cp -- "$BRIDGE" "$ARCHIVE"
cmp --silent "$BRIDGE" "$ARCHIVE"
bash "$D/tests/run.sh"
echo '[STAGE26] Independent EFI outputs (no USB writes):'
sha256sum "$BRIDGE" "$ARCHIVE"
echo 'Stage20 expects USB EFI/BOOT/RTX3080-STAGE19-ENGINE.EFI; do not confuse the filename with the policy version.'
echo 'Build/mocks are NOT a hardware safety or repeated-boot validation.'
