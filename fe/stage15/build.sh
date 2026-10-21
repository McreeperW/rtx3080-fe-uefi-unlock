#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
HEADERS="${EDK2_HEADERS:-$ROOT/deps/edk2/MdePkg/Include}"
[[ -f "$HEADERS/Uefi.h" ]] || { echo "Missing EDK2 headers: $HEADERS" >&2; exit 2; }
for x in clang lld-link sha256sum; do command -v "$x" >/dev/null || exit 3; done
mkdir -p "$HERE/out"
flags=(-target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -Wno-unused-parameter -I "$HEADERS" -I "$HEADERS/X64")
clang "${flags[@]}" -c "$HERE/src/bootbridge.c" -o "$HERE/out/bootbridge.obj"
clang "${flags[@]}" -c "$HERE/src/guard.c" -o "$HERE/out/guard.obj"
lld-link /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf /out:"$HERE/out/BOOTX64.EFI" "$HERE/out/bootbridge.obj" "$HERE/out/guard.obj"
sha256sum "$HERE/out/BOOTX64.EFI"
