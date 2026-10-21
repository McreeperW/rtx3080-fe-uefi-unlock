#!/usr/bin/env python3
"""Read-only, exact-size+SHA Stage21 EFI and firmware asset verifier.

Checks file identity, NOT device/BIOS/UEFI safety or proof of unlock.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

EXPECTED_EFI = {
    'BOOTX64.EFI': 'e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00',
    'RTX3080-UNLOCK.EFI': '8ef922e0804bf5b0d68f028e8af2c77b676ebfb9b0b3307b7311516e8747d4c5',
    # Filename retained for compatibility: its content must be the Stage21 engine.
    'RTX3080-STAGE19-ENGINE.EFI': '0d578a0612fb8a59b7320a0e038d2f19b03f9573b2a995f10ed44e6e10b32373',
}

def checksum(path):
    digest = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024*1024), b''):
            digest.update(chunk)
    return digest.hexdigest()

def check(path, expected_sha, expected_size=None):
    if path.is_symlink() or not path.is_file():
        print('FAIL missing/unsafe:', path)
        return False
    if expected_size is not None and path.stat().st_size != expected_size:
        print('FAIL size:', path.name, f'expected={expected_size} got={path.stat().st_size}')
        return False
    actual=checksum(path)
    ok=actual.lower()==expected_sha.lower()
    print(('OK' if ok else 'FAIL sha256')+': '+str(path))
    if not ok: print('   sha256='+actual+' expected='+expected_sha)
    return ok

def main():
    ap=argparse.ArgumentParser(description='Read-only Stage21 three-EFI / five-firmware verifier')
    group=ap.add_mutually_exclusive_group(required=True)
    group.add_argument('--usb',type=Path,help='Mounted USB root, e.g. /mnt/f')
    group.add_argument('--built',action='store_true',help='Verify fresh outputs under fe/stage* (build != validated)')
    group.add_argument('--efi-dir',type=Path,help='Folder containing three *verified* EFI binaries')
    ap.add_argument('--assets-dir',type=Path,help='Local firmware directory (required with --efi-dir)')
    ap.add_argument('--require-arm',action='store_true',help='Fail unless one-shot marker is present')
    args=ap.parse_args()
    root=Path(__file__).resolve().parents[1]
    if args.usb:
        if not args.usb.is_dir():
            ap.error('USB path does not exist; mount it first')
        efi_base=args.usb/'EFI'/'BOOT'
        assets=args.usb/'RTX3080-UNLOCK'/'ASSETS'
        paths={name:efi_base/name for name in EXPECTED_EFI}
    elif args.built:
        paths={
         'BOOTX64.EFI':root/'fe/stage15/out/BOOTX64.EFI',
         'RTX3080-UNLOCK.EFI':root/'fe/stage20/out/RTX3080-UNLOCK.EFI',
         'RTX3080-STAGE19-ENGINE.EFI':root/'fe/stage21/out/RTX3080-STAGE19-ENGINE.EFI',
        }
        assets=args.assets_dir
    else:
        if args.assets_dir is None:
            ap.error('--assets-dir is required with --efi-dir')
        paths={name:args.efi_dir/name for name in EXPECTED_EFI}
        assets=args.assets_dir
    passed=True
    print('=== Exact known-validated EFI binary SHA-256 comparison ===')
    for name,expected in EXPECTED_EFI.items():
        passed=check(paths[name], expected) and passed
    if assets is not None:
        try:
            manifest=json.loads((root/'unlock/assets.json').read_text(encoding='utf-8'))
            if len(manifest)!=5 or len({m['Name'] for m in manifest})!=5:
                raise ValueError('expected exactly five unique asset records')
        except (OSError,ValueError,KeyError) as err:
            print('FAIL: asset manifest:',err)
            return 2
        print('=== Exact five firmware asset size + SHA-256 comparison ===')
        for m in manifest:
            passed=check(assets/m['Name'],m['Sha256'],m['Bytes']) and passed
    else:
        print('WARNING: firmware assets NOT checked; use --assets-dir or --usb.')
        passed=False
    if args.usb:
        marker=args.usb/'RTX3080-STAGE15-ARM.ONCE'
        if marker.exists():
            if not marker.is_file() or marker.read_bytes()!=b'I_ACCEPT_GPU_HANG_RISK':
                print('FAIL: invalid one-shot marker')
                passed=False
            else:
                print('ARMED: one-shot marker present (NEXT BOOT WILL ATTEMPT FIRMWARE TRANSACTION)')
        else:
            print('NOT ARMED: no one-shot marker (Stage15 should skip engine)')
            if args.require_arm: passed=False
    if passed:
        print('PASS: all expected files match known successful-run hashes. NOT A CROSS-HARDWARE SAFETY CERTIFICATION.')
        return 0
    print('FAIL: mismatched/missing files or marker requirement. DO NOT ARM THIS USB.')
    return 1

if __name__=='__main__':
    sys.exit(main())
