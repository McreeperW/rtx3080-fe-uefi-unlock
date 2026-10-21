#!/usr/bin/env bash
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd)
ROOT=${PROJECT_ROOT:-"$(cd "$D/../.." && pwd)"}
EDK="$ROOT/deps/edk2/MdePkg/Include"
[[ -f "$EDK/Uefi.h" ]] || exit 2
mkdir -p "$D/out"
clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -Wno-unused-parameter -I "$EDK" -I "$EDK/X64" -c "$D/src/bridge.c" -o "$D/out/stage20-bridge.obj"
lld-link /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf /out:"$D/out/RTX3080-UNLOCK.EFI" "$D/out/stage20-bridge.obj"
sha256sum "$D/out/RTX3080-UNLOCK.EFI"
