#!/usr/bin/env bash
# Install the experimental persistent-auto Stage15 loader on an already verified,
# successful Stage21 USB. Never touch the Windows ESP or Stage20/21 binaries.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
USB="${1:-}"
if [[ -z "$USB" || "$USB" == '/' || ! -d "$USB/EFI/BOOT" || ! -d "$USB/RTX3080-UNLOCK/ASSETS" ]]; then
  echo 'Usage: bash scripts/install-auto-usb.sh /mnt/f (existing verified USB)' >&2
  exit 2
fi
for cmd in python3 sha256sum cp mv mktemp sync; do
  command -v "$cmd" >/dev/null || { echo "ERROR: missing $cmd" >&2; exit 2; }
done
# Exact observed binaries; never quietly replace an unrecognized loader.
ONE_SHOT='e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00'
AUTO='2614d688d29f85cd4ff083d03e6abe2338e77f1851efc6fdbb88a93f867b2171'
NEW="$ROOT/fe/stage15/out/BOOTX64.EFI"
TARGET="$USB/EFI/BOOT/BOOTX64.EFI"
BACKUP="$USB/RTX3080-STAGE15-BOOTX64-ONESHOT.BAK"
# Only the tested Stage20+Stage21 and asset hashes are admissible.
python3 "$ROOT/scripts/check-usb.py" --usb "$USB"
NEW_SHA="$(sha256sum "$NEW" | awk '{print $1}')"
[[ "$NEW_SHA" == "$AUTO" ]] || { echo 'ERROR: new Stage15 EFI does not match the packaged AUTO source output.' >&2; exit 3; }
OLD_SHA="$(sha256sum "$TARGET" | awk '{print $1}')"
if [[ "$OLD_SHA" == "$ONE_SHOT" ]]; then
  if [[ -e "$BACKUP" ]]; then
    [[ "$(sha256sum "$BACKUP" | awk '{print $1}')" == "$ONE_SHOT" ]] || { echo 'ERROR: conflicting backup. Stop.' >&2; exit 3; }
  else
    cp -- "$TARGET" "$BACKUP"
  fi
elif [[ "$OLD_SHA" == "$AUTO" ]]; then
  echo 'INFO: AUTO Stage15 already installed.'
else
  echo "ERROR: existing Stage15 unknown: $OLD_SHA" >&2; exit 3
fi
if [[ -e "$USB/RTX3080-STAGE15-AUTO.PENDING" ]]; then
  echo 'ERROR: AUTO.PENDING exists. A previous attempt may have failed. Not re-arming.' >&2
  exit 4
fi
if [[ -e "$USB/RTX3080-STAGE15-ARM.ONCE" ]]; then
  echo 'ERROR: existing one-shot marker present. Remove it manually after reviewing status.' >&2
  exit 4
fi
# Stage15 is the ONLY EFI file changed. Backup remains on USB.
if [[ "$OLD_SHA" != "$AUTO" ]]; then
  STAGED="$USB/EFI/BOOT/BOOTX64.EFI.new"
  [[ ! -e "$STAGED" ]] || { echo 'ERROR: unexpected staging file exists. Stop.' >&2; exit 4; }
  cp -- "$NEW" "$STAGED"
  echo "$AUTO  $STAGED" | sha256sum -c -
  mv -- "$STAGED" "$TARGET"
  sync
fi
[[ "$(sha256sum "$TARGET" | awk '{print $1}')" == "$AUTO" ]] || { echo 'ERROR: Stage15 installation verification failed' >&2; exit 5; }
# Token is NOT a zero-byte marker. Persistent opt-in: never consumed by UEFI.
if [[ -e "$USB/RTX3080-STAGE15-AUTO.ON.new" ]]; then
  echo 'ERROR: stale AUTO.ON.new marker exists. Stop.' >&2
  exit 5
fi
if [[ -e "$USB/RTX3080-STAGE15-AUTO.ON" ]] &&
   [[ "$(cat "$USB/RTX3080-STAGE15-AUTO.ON")" != 'I_ACCEPT_REPEAT_GPU_HANG_RISK' ]]; then
  echo 'ERROR: malformed existing AUTO.ON marker. Stop.' >&2
  exit 5
fi
printf 'I_ACCEPT_REPEAT_GPU_HANG_RISK' > "$USB/RTX3080-STAGE15-AUTO.ON.new"
mv -- "$USB/RTX3080-STAGE15-AUTO.ON.new" "$USB/RTX3080-STAGE15-AUTO.ON"
sync
python3 "$ROOT/scripts/check-usb.py" --usb "$USB" --require-auto
printf '\nPASS: AUTO is enabled for USB boot; Stage20 and Stage21 unchanged.\n'
echo 'SAFETY: repeated USB boots can rerun risky GPU transactions; unplug USB to bypass.'
echo 'A hang leaves AUTO.PENDING and blocks a further automatic attempt.'
