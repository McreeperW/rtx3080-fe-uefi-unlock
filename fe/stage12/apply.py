#!/usr/bin/env python3
"""Source-version-locked Stage 12 experimental opt-in overlay; no GPU access."""
from pathlib import Path
import hashlib,shutil,sys
root=Path(__file__).resolve().parents[2]
expected={
 'app.c':'8ec548a9b571f13da2926e9685c0cd32b2c725a50be68ff05a35e5ab432d1e2b',
 'checks.c':'f6a6571dc6daa8f02301b311a027be50064e14414d5bfb5864a91f0cc0b1eeb3',
 'port.c':'4438bfba2b3b73b81cd86f3014d06871a9f4ff4604f70426d328cbc2125ecbcc',
}
for name,digest in expected.items():
    dst=root/'unlock'/name
    if not dst.is_file() or hashlib.sha256(dst.read_bytes()).hexdigest()!=digest:
        sys.exit(f'Refusing Stage12 patch: unknown source revision at {dst}')
back=root/'fe/stage12/backup';back.mkdir(exist_ok=True)
for name in expected:
    dst=root/'unlock'/name
    shutil.copy2(dst,back/name)
    shutil.copy2(root/'fe/stage12/payload'/name,dst)
print('Stage12 source overlay applied. Default build remains FE-readonly; use explicit experimental builder to enable writes.')
