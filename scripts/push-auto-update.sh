#!/usr/bin/env bash
# Update ONLY the approved files and push to the configured origin (never force).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
[[ -d .git ]] || { echo 'ERROR: not a Git clone' >&2; exit 2; }
git remote get-url origin >/dev/null || { echo 'ERROR: no origin remote' >&2; exit 2; }
if ! git diff --cached --quiet; then
 echo 'ERROR: other files already staged; review git status before committing.' >&2; exit 2
fi
FILES=(README.md docs/AUTO_USB_KO.md docs/TUTORIAL_KO.md \
  fe/stage15/src/bootbridge.c scripts/build-all.sh scripts/check-usb.py \
  scripts/install-auto-usb.sh scripts/disable-auto-usb.sh scripts/push-auto-update.sh)
git add -- "${FILES[@]}"
if git diff --cached --quiet; then echo 'No changes to commit.'; exit 0; fi
printf '\nStaged changes:\n'
git diff --cached --stat
printf '\nRemote: '; git remote get-url origin
read -r -p 'Commit and push these exact files? [y/N] ' answer
[[ "$answer" == y || "$answer" == Y ]] || { git reset -- "${FILES[@]}"; echo 'Cancelled.'; exit 1; }
git commit -m 'feat: opt-in persistent USB unlock with crash retry guard'
branch="$(git branch --show-current)"
[[ -n "$branch" ]] || { echo "ERROR: detached HEAD; push manually" >&2; exit 3; }
git push -u origin "$branch"
