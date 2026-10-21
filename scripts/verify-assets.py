#!/usr/bin/env python3
"""Verify RTX 3080 FE Stage21 firmware input files, without touching hardware."""
import argparse
import hashlib
import json
from pathlib import Path
import sys


def digest(path):
    sha = hashlib.sha256()
    with path.open("rb") as stream:
        while True:
            data = stream.read(1024 * 1024)
            if not data:
                return sha.hexdigest()
            sha.update(data)


def main():
    parser = argparse.ArgumentParser(description="Read-only 5-file asset size and SHA-256 check")
    parser.add_argument("assets_dir", type=Path, help="Directory containing five .bin files")
    args = parser.parse_args()
    manifest = Path(__file__).resolve().parents[1] / "unlock" / "assets.json"
    try:
        items = json.loads(manifest.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as err:
        print(f"FAIL: cannot read manifest: {err}", file=sys.stderr)
        return 2
    if len(items) != 5 or len({x['Name'] for x in items}) != 5:
        print("FAIL: unexpected asset manifest structure", file=sys.stderr)
        return 2
    if not args.assets_dir.is_dir():
        print(f"FAIL: missing asset directory: {args.assets_dir}", file=sys.stderr)
        return 1
    failed = 0
    for item in items:
        name = item['Name']
        path = args.assets_dir / name
        if path.is_symlink() or not path.is_file():
            print(f"FAIL: missing/not regular file: {name}")
            failed += 1
            continue
        size = path.stat().st_size
        if size != item['Bytes']:
            print(f"FAIL: {name}: size {size} != {item['Bytes']}")
            failed += 1
            continue
        sha = digest(path)
        if sha.lower() != item['Sha256'].lower():
            print(f"FAIL: {name}: SHA-256 mismatch ({sha})")
            failed += 1
            continue
        print(f"OK: {name} ({size} bytes, SHA-256 match)")
    if failed:
        print(f"FAIL: {failed}/5 asset(s) missing or mismatched")
        return 1
    print("PASS: all 5 assets verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
