#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HEADERS="${EDK2_HEADERS:-$HERE/../../deps/edk2/MdePkg/Include}"
for x in clang lld-link python3 sha256sum; do command -v "$x" >/dev/null || { echo "missing: $x"; exit 2; }; done
[[ -f "$HEADERS/Uefi.h" ]] || { echo "Missing headers; export EDK2_HEADERS=/path/to/deps/edk2/MdePkg/Include"; exit 3; }
mkdir -p "$HERE/out"
flags=(-target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -Wno-unused-parameter -I "$HEADERS" -I "$HEADERS/X64")
clang "${flags[@]}" -c "$HERE/src/bootbridge.c" -o "$HERE/out/bootbridge.obj"
clang "${flags[@]}" -c "$HERE/src/guard.c" -o "$HERE/out/guard.obj"
lld-link /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf /out:"$HERE/out/BOOTX64.EFI" "$HERE/out/bootbridge.obj" "$HERE/out/guard.obj"
sha256sum "$HERE/out/BOOTX64.EFI"
