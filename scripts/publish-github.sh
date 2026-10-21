#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
NAME="${GITHUB_REPO_NAME:-rtx3080-fe-uefi-unlock}"
VIS=private
if [[ "${1:-}" == '--public' ]];then VIS=public
elif [[ "${1:-}" != '' ]];then echo '사용법: bash scripts/publish-github.sh [--public]' >&2;exit 2
fi
for cmd in git gh;do command -v "$cmd" >/dev/null || { echo "ERROR: $cmd not found" >&2;exit 3;};done
if ! gh auth status >/dev/null 2>&1;then
  echo 'GitHub 웹 브라우저 로그인이 필요합니다.'
  gh auth login --web --git-protocol https
fi
gh auth setup-git >/dev/null
ACCOUNT=$(gh api user --jq '.login')
[[ -n "$ACCOUNT" ]] || { echo 'ERROR: GitHub account lookup failed' >&2;exit 4; }
[[ ! -d .git ]] && git init -b main
[[ -z "$(git remote)" ]] || { echo 'ERROR: 원격 Git 저장소가 이미 연결되어 있습니다. 자동 덮어쓰기 금지.' >&2;exit 5; }
git config user.name "$ACCOUNT"
git config user.email "${ACCOUNT}@users.noreply.github.com"
git add -A
if git diff --cached --name-only | grep -Ei '(^|/)(ASSETS|gpu-unlock|EFI|blobs)/|\.(efi|bin|obj|log)$|(^|/)REPORT.*\.TXT$';then
  echo 'ERROR: 제외해야 할 파일이 스테이징되어 있습니다. git reset 후 점검하세요.' >&2;exit 6
fi
if git diff --cached --quiet;then echo 'ERROR: 커밋할 파일이 없습니다.' >&2;exit 7;fi
printf '
업로드 대상: GitHub 계정 %s / 저장소 %s (%s)
' "$ACCOUNT" "$NAME" "$VIS"
git diff --cached --stat | tail -8
printf '
위 파일을 확인했습니다. 공개 설정은 %s 입니다.
' "$VIS"
git commit -m "docs: publish RTX 3080 FE Stage21 research sources"
gh repo create "$ACCOUNT/$NAME" "--$VIS" --source=. --remote=origin --push
printf '
등록 완료: https://github.com/%s/%s
' "$ACCOUNT" "$NAME"
