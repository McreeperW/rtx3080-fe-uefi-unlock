#!/usr/bin/env python3
"""Restore Stage11 guarded source from the local Stage12 backup only."""
from pathlib import Path
import hashlib, shutil, sys
root=Path(__file__).resolve().parents[2]
new={
'app.c':'3bcfaf0f0981f8bfcb95583fdb622c846f6c50b0681f33c9f08aa161d85e4274',
'checks.c':'f0e794da258ce65acec7eb19baec47318a535933dfdb9c3bf4db5f903f7df21f',
'port.c':'fa6ccdaeb1bdacf89b9be32a9caf87a41e1e653b931f09e6d3562675c2fa263f',
}
original={
'app.c':'8ec548a9b571f13da2926e9685c0cd32b2c725a50be68ff05a35e5ab432d1e2b',
'checks.c':'f6a6571dc6daa8f02301b311a027be50064e14414d5bfb5864a91f0cc0b1eeb3',
'port.c':'4438bfba2b3b73b81cd86f3014d06871a9f4ff4604f70426d328cbc2125ecbcc',
}
for name in new:
 p=root/'unlock'/name
 b=root/'fe/stage12/backup'/name
 for path,digest in ((p,new[name]),(b,original[name])):
  if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
   sys.exit('Restore aborted: unexpected file content at '+str(path))
for name in new: shutil.copy2(root/'fe/stage12/backup'/name,root/'unlock'/name)
print('Stage11 guarded sources restored; run bash fe/stage5/build-wsl.sh for guarded EFI.')
