#!/usr/bin/env bash
set -euo pipefail
# The current repo policy is Stage26. The earlier Stage21-specific pinned
# PE hash and strict allowlist assertions are archived in Git history.
D=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
exec bash "$D/run-stage26.sh"
