#!/usr/bin/env bash
set -euo pipefail
D=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SRC="$D/src/bridge.c"
BIN="$D/out/RTX3080-UNLOCK.EFI"
bash -n "$D/install-once.sh" "$D/restore-stage18.sh" "$D/build.sh"
test "$(sha256sum "$BIN" | cut -d' ' -f1)" = 8ef922e0804bf5b0d68f028e8af2c77b676ebfb9b0b3307b7311516e8747d4c5
file "$BIN" | grep -q 'PE32+ executable for EFI (application), x86-64'
python3 - "$SRC" <<'PY'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
assert s.count('ConnectController(gpu,NULL,NULL,TRUE)')==2  # once in comment, once in unexecuted path after postengine check
assert s.index('if(engine_returned){\n    reconnect_skipped_post_engine=TRUE;') < s.index('EFI_STATUS connect_status=ST->BootServices->ConnectController')
assert 'goto finish;' in s[s.index('if(engine_returned){'):s.index('EFI_STATUS connect_status=')]
assert 'stage20_result=PASS_UNLOCK_SELECTORS_SKIP_GOP_RECONNECT_WINDOWS_UNVERIFIED' in s
assert 'RTX3080-STAGE20-UNLOCK.TXT' in s
assert 'engine_result==EFI_SUCCESS' in s
print('PASS: Stage20 compiled binary, bypass ordering, logging, one-shot installer syntax')
PY
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/EFI/BOOT"
cp "$D/out/RTX3080-STAGE19-UNLOCK.EFI" "$TMP/EFI/BOOT/RTX3080-UNLOCK.EFI"
if "$D/install-once.sh" "$TMP" >/dev/null 2>&1; then echo 'FAIL: unverified fixture accepted';exit 1;fi
[[ ! -e "$TMP/RTX3080-STAGE15-ARM.ONCE" ]]
echo 'PASS: fail-closed missing bridge fixture'
