#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Apply ASSETS provenance docs and push ONLY these files to the current Git branch.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
git rev-parse --is-inside-work-tree >/dev/null
if ! git diff --quiet || ! git diff --cached --quiet; then
  echo 'STOP: tracked changes/staged changes exist. Commit or stash before using this helper.' >&2
  exit 1
fi
python3 scripts/update-assets-docs.py
git diff --check
git add -- README.md docs/ASSETS_GUIDE_KO.md docs/TUTORIAL_KO.md scripts/update-assets-docs.py scripts/push-assets-docs.sh
if git diff --cached --quiet; then
  echo 'No documentation changes to push.'
  exit 0
fi
echo '=== Commit summary ==='
git diff --cached --stat
git commit -m 'docs: document original NVIDIA asset generation via make assets'
git push
echo 'DONE: documentation pushed.'
