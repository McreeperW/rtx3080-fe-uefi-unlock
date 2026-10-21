#!/usr/bin/env python3
"""Read-only, exact-size+SHA Stage21 EFI and firmware asset verifier.

Checks file identity, NOT device/BIOS/UEFI safety or proof of unlock.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ONE_SHOT_BOOT_SHA = 'e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00'
AUTO_BOOT_SHA = '2614d688d29f85cd4ff083d03e6abe2338e77f1851efc6fdbb88a93f867b2171'
AUTO_TOKEN = b'I_ACCEPT_REPEAT_GPU_HANG_RISK'

EXPECTED_EFI = {
    'BOOTX64.EFI': ONE_SHOT_BOOT_SHA,
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
    ap.add_argument('--require-arm',action='store_true',help='Fail unless a valid one-shot marker is present')
    ap.add_argument('--require-auto',action='store_true',help='Fail unless AUTO enabled and no previous attempt lock')
    args=ap.parse_args()
    if args.require_auto and not args.usb:
        ap.error('--require-auto requires --usb')
    if args.require_arm and not args.usb:
        ap.error('--require-arm requires --usb')
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
    print('=== Exact Stage21 EFI binary SHA-256 comparison ===')
    for name,expected in EXPECTED_EFI.items():
        if name == 'BOOTX64.EFI' and paths[name].is_file():
            got=checksum(paths[name])
            if got == AUTO_BOOT_SHA and not paths[name].is_symlink():
                print('OK: '+str(paths[name])+' (persistent auto Stage15: EXPERIMENTAL / GPU UNTESTED)')
                continue
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
        one_shot=False
        if marker.exists():
            if not marker.is_file() or marker.read_bytes()!=b'I_ACCEPT_GPU_HANG_RISK':
                print('FAIL: invalid one-shot marker')
                passed=False
            else:
                one_shot=True
                print('ARMED: one-shot marker present')
        auto=args.usb/'RTX3080-STAGE15-AUTO.ON'
        pending=args.usb/'RTX3080-STAGE15-AUTO.PENDING'
        auto_ok=False
        if auto.exists():
            if not auto.is_file() or auto.read_bytes()!=AUTO_TOKEN:
                print('FAIL: malformed AUTO marker')
                passed=False
            elif not paths['BOOTX64.EFI'].is_file() or checksum(paths['BOOTX64.EFI']) != AUTO_BOOT_SHA:
                print('FAIL: AUTO marker requires the AUTO Stage15 loader')
                passed=False
            elif pending.exists():
                print('BLOCKED: AUTO.PENDING exists (previous attempt may have failed). Do not bypass casually.')
            else:
                auto_ok=True
                print('ARMED: persistent AUTO marker present (UNTESTED new loader)')
        elif pending.exists():
            print('INFO: stale AUTO.PENDING remains while auto is disabled')
        if args.require_arm and not one_shot: passed=False
        if args.require_auto and not auto_ok: passed=False
        if one_shot and auto.exists():
            print('FAIL: conflicting one-shot and AUTO modes')
            passed=False
        if not one_shot and not auto.exists():
            print('NOT ARMED: no auto/one-shot marker')
    if passed:
        print('PASS: expected file identities and assets. AUTO Stage15 is newly compiled and NOT GPU-VALIDATED.')
        return 0
    print('FAIL: mismatched/missing files or marker requirement. DO NOT ARM THIS USB.')
    return 1

if __name__=='__main__':
    sys.exit(main())
