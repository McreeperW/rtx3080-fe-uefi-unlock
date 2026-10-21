#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HEADERS="${EDK2_HEADERS:-$ROOT/deps/edk2/MdePkg/Include}"
for x in clang lld-link python3 sha256sum; do command -v "$x" >/dev/null || { echo "missing: $x"; exit 2; }; done
[[ -f "$HEADERS/Uefi.h" ]] || { echo "Missing EDK2 MdePkg headers; set EDK2_HEADERS=/path/to/deps/edk2/MdePkg/Include"; exit 3; }
mkdir -p "$ROOT/out"
clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -Wno-unused-parameter -I "$HEADERS" -I "$HEADERS/X64" -c "$ROOT/src/bootbridge.c" -o "$ROOT/out/bootbridge.obj"
lld-link /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf /out:"$ROOT/out/BOOTX64.EFI" "$ROOT/out/bootbridge.obj"
sha256sum "$ROOT/out/BOOTX64.EFI"
