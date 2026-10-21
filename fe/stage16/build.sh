#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
EDK=${EDK2_INCLUDE:-"$ROOT/../../deps/edk2/MdePkg/Include"}
if [[ ! -f "$EDK/Uefi.h" ]]; then
  echo "Pass EDK2_INCLUDE=/path/to/deps/edk2/MdePkg/Include" >&2;exit 2
fi
clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -Wno-unused-parameter -I "$EDK" -I "$EDK/X64" -c "$ROOT/src/preflight.c" -o "$ROOT/out/preflight.obj"
lld-link /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf /out:"$ROOT/out/RTX3080-UNLOCK.EFI" "$ROOT/out/preflight.obj"
sha256sum "$ROOT/out/RTX3080-UNLOCK.EFI"
