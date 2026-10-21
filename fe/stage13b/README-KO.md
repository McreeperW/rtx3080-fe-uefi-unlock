# RTX 3080 FE Stage 13b: Boot0000 LoadImage EFI_NOT_FOUND fix

## Confirmed real-hardware failure

The Stage 13 report showed `sole_gpu_topology=OBSERVED`, `windows_boot_option=Boot0000`, and `windows_loadimage_status=0x800000000000000e` (`EFI_NOT_FOUND`). GPU detection succeeded; Windows Boot Manager was found in UEFI BootOrder, but directly passing its possibly abbreviated HD/File device path to `LoadImage()` failed.

## Change

- Keep the existing `Boot0000` selection and initial direct `LoadImage()`.
- Only after `EFI_NOT_FOUND`, parse the **exact hard-drive node** of the already validated Windows Boot Manager boot option.
- Enumerate `EFI_SIMPLE_FILE_SYSTEM_PROTOCOL` handles and require an exact matching HD node, including the partition signature and metadata.
- Open `\\EFI\\Microsoft\\Boot\\bootmgfw.efi` **read-only** on the matched partition. Construct a full device path using the filesystem handle's complete path plus the Windows boot manager file node. Retry `LoadImage()` with this full path; call `StartImage()` on success.
- Never select a filesystem without an exact hard-drive node match. Never modify the Windows EFI file, BootOrder, or NVRAM.
- Log `fallback_fs_enum_status`, `fallback_fs_handles`, `fallback_matching_hd_handles`, `fallback_bootmgfw_open_status`, `fallback_complete_path_loadimage_status`, and `windows_final_loadimage_status`.

**This is not a GPU unlock.** It does not perform GPU writes, DMA, SEC2/GSP, MSE/BME, or FLR. It only tests automatic Windows boot handoff on Ryzen 5 3600 + RTX 3080 FE with the GPU as active display.

## USB install

Windows `F:` (WSL `/mnt/f`) must be the verified FAT32 removable USB:

```bash
cd ~/code/rtx3080-fe-reference/rtx3080-unlock-public-main
unzip -o /mnt/c/Users/pc/Downloads/rtx3080-fe-stage13b-bootfix.zip -d .
bash fe/stage13b/install-usb.sh /mnt/f
```

This backs up the currently installed EFI as `EFI/BOOT/BOOTX64.STAGE13-BACKUP.EFI` if no backup already exists. It does **not** change firmware boot priorities or the Windows disk. If USB mounting fails, use Windows PowerShell to copy the EFI instead of running `sudo` blindly.

Boot USB once. If Windows starts automatically, the Stage 13 boot bridge handoff is verified. If it returns to the firmware boot menu, boot Windows normally and upload the most recent `F:\RTX3080-BOOT-*.TXT` file. Preserve the full report rather than only filtered fields.

## Restore Stage 13

```bash
bash fe/stage13b/restore-usb.sh /mnt/f
```

## Rebuild in WSL

```bash
EDK2_HEADERS="$PWD/deps/edk2/MdePkg/Include" bash fe/stage13b/build.sh
```

Tested: Clang freestanding PE32+ EFI build and simulated shortened-HD/full-HD path comparisons. Actual Windows boot handoff in the user's firmware remains unverified.
