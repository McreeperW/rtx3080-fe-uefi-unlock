#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
EDK=${EDK2_INCLUDE:-"$ROOT/../../deps/edk2/MdePkg/Include"}
[[ -f "$EDK/Uefi.h" ]] || { echo 'ERROR: set EDK2_INCLUDE for MdePkg/Include' >&2;exit 2; }
mkdir -p "$ROOT/out"
clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -Wno-unused-parameter -I "$EDK" -I "$EDK/X64" -c "$ROOT/src/snapshot.c" -o "$ROOT/out/snapshot.obj"
lld-link /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf /out:"$ROOT/out/RTX3080-UNLOCK.EFI" "$ROOT/out/snapshot.obj"
sha256sum "$ROOT/out/RTX3080-UNLOCK.EFI"
