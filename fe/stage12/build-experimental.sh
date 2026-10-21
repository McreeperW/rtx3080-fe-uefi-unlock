#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
for t in clang lld-link python3 sha256sum; do command -v "$t" >/dev/null || { echo "Missing tool: $t" >&2; exit 2; }; done
# Reject accidental use against a differently structured or de-guarded tree.
grep -Fq 'GU_FE_EXPERIMENTAL' unlock/app.c || { echo 'FE experiment source absent' >&2; exit 3; }
grep -Fq 'FeEnableDecode()' unlock/app.c || { echo 'FE decode source absent' >&2; exit 3; }

mkdir -p unlock/build
sources=(app app_io ownership format runtime sha256 checks port compat vendor/unlock_610 vendor/prep_610 vendor/layout_610 vendor/vbios_610 vendor/gfw_eval)
objects=()
for source in "${sources[@]}"; do
  object="unlock/build/${source//\//_}.obj"
  clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -Wno-unused-parameter -D__KERNEL__ -DGU_UEFI_PORT -DGU_FE_EXPERIMENTAL -DGU_TARGET_PCI_ID=0x220610deU -I unlock/compat -I deps/edk2/MdePkg/Include -I deps/edk2/MdePkg/Include/X64 -c "unlock/${source}.c" -o "$object"
  objects+=("$object")
done
lld-link /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf /map:unlock/build/BOOTX64.map /out:unlock/build/BOOTX64.EFI "${objects[@]}"
python3 unlock/verify_pe.py unlock/build/BOOTX64.EFI unlock/build/BOOTX64.map --output unlock/build/pe-verification.json
mkdir -p fe/stage12/build
cp unlock/build/BOOTX64.EFI fe/stage12/build/BOOTX64.FE-EXPERIMENTAL.EFI
sha256sum fe/stage12/build/BOOTX64.FE-EXPERIMENTAL.EFI
printf '\nBUILT: %s\n' "$ROOT/unlock/build/BOOTX64.EFI"
printf 'DANGER: FE EXPERIMENTAL real GPU write/FLR path is enabled. Unvalidated on hardware.\n'
