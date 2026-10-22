#!/usr/bin/env bash
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd)
BASE=${PROJECT_ROOT:-"$(cd "$D/../.." && pwd)"}
EDK="$BASE/deps/edk2/MdePkg/Include"
[[ -f "$EDK/Uefi.h" ]] || { echo 'ERROR: EDK2 MdePkg headers unavailable' >&2; exit 2; }
mkdir -p "$D/out/objects"
sources=(app app_io ownership format runtime sha256 checks port compat vendor/unlock_610 vendor/prep_610 vendor/layout_610 vendor/vbios_610 vendor/gfw_eval)
objects=()
for s in "${sources[@]}"; do
  path="$D/engine/${s}.c"
  [[ -f "$path" ]] || path="$BASE/unlock/${s}.c"
  [[ -f "$path" ]] || { echo "Missing source: $s"; exit 3; }
  obj="$D/out/objects/${s//\//_}.obj"
  clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -Wno-unused-parameter -D__KERNEL__ -DGU_UEFI_PORT -DGU_FE_EXPERIMENTAL -DGU_FE_HEADLESS -DGU_TARGET_PCI_ID=0x220610deU -I "$D/engine" -I "$D/engine/compat" -I "$EDK" -I "$EDK/X64" -c "$path" -o "$obj"
  objects+=("$obj")
done
lld-link /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf /out:"$D/out/RTX3080-STAGE19-ENGINE.EFI" "${objects[@]}"
sha256sum "$D/out/RTX3080-STAGE19-ENGINE.EFI"
