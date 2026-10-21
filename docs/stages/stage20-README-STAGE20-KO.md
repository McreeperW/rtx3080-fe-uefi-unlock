# RTX 3080 FE Stage20 — NO GOP RECONNECT (experimental)

## Why this exists

The actual Stage19 hardware logs showed success **through the unlock engine**:

- `REPORT.TXT`: `transaction result=0`, `FLR=1`, `FINAL status=0`, `clean=1`
- `RTX3080-STAGE19-UNLOCK.TXT`: `stage19_engine_return_status=0`, `stage19_post_engine_ss0=0x88888888`, `stage19_post_engine_ss1=0x8`, PCI Memory Decode restored to `0x0`.
- Last logged stage: `stage18_reconnection_attempt=1` **without** any `stage18_connect_status`.

The likely non-returning operation is `ConnectController(gpu,NULL,NULL,TRUE)` in the post-FLR firmware graphics-driver rebind. This is not conclusively proven, since no timer or CPU-level trace was captured. The prior black screen required forced power-off and after that the unlock state would reset.

## Stage20 change

- Keep Stage15's already-verified USB boot bridge and preloaded Windows Boot Manager.
- Keep the exact previously successful Stage19 SEC2/GSP/FWSEC/FLR engine binary and verified firmware assets.
- Keep Stage19 ownership checks, PCI Command = 0x2 pre-engine, selector readback, and clean engine exit.
- **Only when the engine has returned**, do NOT try `ConnectController()` after its FLR. Record `stage20_reconnect_skipped=POST_ENGINE_FLR_OR_UNKNOWN_STATE`, and return to the Stage15 bridge.
- If the engine never ran, the Stage17-proven reconnect path remains (pre-engine failure recovery); do not skip it.
- Run only once via Stage15's consumed-on-entry `RTX3080-STAGE15-ARM.ONCE` marker. No background retries.

**Critical limit**: Neither Windows Boot Manager's ability to run without the NVIDIA GOP nor NVIDIA display recovery in Windows has been tested. Skipping GOP reconnection might still yield a black screen, hang, failure to boot, or driver Code 43. The firmware transaction can itself hang. Preloading Windows does NOT constitute a guaranteed recovery mechanism.

## Installation (WSL, assuming F: is the USB)

Extract the ZIP into the existing repository first:

```bash
cd ~/code/rtx3080-fe-reference/rtx3080-unlock-public-main
unzip -o '/mnt/c/Users/pc/Downloads/rtx3080-fe-stage20-no-gop-reconnect.zip' -d .
bash fe/stage20/tests/run.sh
bash fe/stage20/install-once.sh /mnt/f
sync
```

Installer requires:

- exact Stage15 `EFI/BOOT/BOOTX64.EFI` (SHA-256 `e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00`)
- exact Stage19 engine `EFI/BOOT/RTX3080-STAGE19-ENGINE.EFI` (SHA-256 `d8da3d26ccd09b37148b20a60ca98fcce6c66eb8c917e56701de4e7561b35ee3`)
- exact Stage18 backup `EFI/BOOT/RTX3080-STAGE18-STAGE19-BACKUP.EFI` (SHA-256 `4be34c5acdff24113a3cfab14e00f7b5438591d0392af9238aacc924da46342b`)
- current child Stage19 or Stage18, exact SHA-256.
- the five previously pinned firmware files in `RTX3080-UNLOCK/ASSETS`.
- NO pending one-shot arm marker or Stage20 prior-install backup.

If checks fail, **do not** overwrite EFI files manually.

Successful installation prints `PASS: Stage20 SKIP GOP reconnect, armed ONCE.`. This only means the USB is staged; it does **not** mean the firmware will boot successfully.

## After test

Inspect USB root logs:

- `RTX3080-STAGE20-UNLOCK.TXT`
- `RTX3080-STAGE15-*.TXT`
- `RTX3080-UNLOCK/RUNxx/REPORT.TXT`

If Windows boots, check NVIDIA GPU detection and the same FP16/BF16 benchmarks as the prior successful experiment. Unlock at the UEFI stage is **not proof** that Windows kept it.

Restore safe Stage18 if the test fails or before any further experimental attempt:

```bash
bash fe/stage20/restore-stage18.sh /mnt/f
sync
```

Remove the USB and cold-boot from the SSD if the screen stays black. Powering off resets the transient unlock, and the user may need another display device if the GPU does not recover. Do not repeat Stage20 automatically.
