# GitHub에 업로드 (Git 설치 사용자용)

## Git과 GitHub 인증은 별개

Git 명령이 설치되어 있어도 **GitHub 원격 저장소 생성 및 인증**은 별도로 필요합니다. GitHub는 일반 계정 비밀번호로 HTTPS `git push`를 받지 않습니다(2021-08부터 중단). GitHub CLI (`gh`)의 브라우저 로그인이나 PAT(개인 액세스 토큰)를 사용하세요. **아이디·비밀번호·토큰을 README/스크립트/Git 커밋에 적거나 채팅으로 전달하지 마세요.**

## 권장: 브라우저 로그인만으로 개인 저장소 만들기

반드시 ZIP을 **기존 거대 프로젝트 내부가 아닌 별도 새 폴더**에 압축 해제하세요. 아래 예시는 ZIP 내 폴더 이름 그대로 사용합니다.

```bash
unzip rtx3080-fe-uefi-unlock-github.zip -d ~/code
cd ~/code/rtx3080-fe-uefi-unlock
# WSL에 gh 미설치라면: sudo apt update && sudo apt install gh
gh auth login --web --git-protocol https
bash scripts/publish-github.sh
```

스크립트는 **GitHub 저장소를 기본적으로 PRIVATE로 생성**하며, 공개할 때는 파일을 검토한 후 `bash scripts/publish-github.sh --public`을 실행할 수 있습니다. **이미 private로 만들어진 저장소를 public으로 자동 전환하지는 않습니다.**

GitHub CLI 로그인은 웹 브라우저에 나오는 장치 코드 승인 방식이라 계정 암호나 토큰을 터미널에 그대로 넣지 않아도 됩니다. CLI는 `gh repo create --source=. --push`로 현재 폴더를 업로드합니다.

## 직접 Git만 이용하는 경우 (원격 저장소는 GitHub 웹에서 미리 생성)

```bash
cd ~/code/rtx3080-fe-uefi-unlock
git init -b main
git config user.name 'YOUR_GITHUB_USERNAME'
git config user.email 'YOUR_GITHUB_USERNAME@users.noreply.github.com'
git add .
git status --short
git commit -m 'docs: publish RTX 3080 FE Stage 21 research sources'
git remote add origin https://github.com/YOUR_GITHUB_USERNAME/rtx3080-fe-uefi-unlock.git
git push -u origin main
```

Git이 사용자명/암호를 물어보면 **암호 칸에 GitHub 계정 암호가 아닌 PAT**를 입력해야 합니다. URL에 토큰을 끼워 넣지 마세요. 가능하면 Git Credential Manager 또는 `gh auth setup-git`을 이용하세요.

## 공개 전 확인

```bash
git status --short
git ls-files | grep -Ei '(^|/)(ASSETS|gpu-unlock|EFI)/|\.(EFI|bin|obj|log)$|REPORT.*\.TXT$' && echo '검토 필요' || true
```

GPL 출처·저작권 표기와 NVIDIA 펌웨어 비포함 방침을 준수하세요. 개인 PC의 로그·계정 이름·시리얼·파일 경로 등은 공개 저장소에 추가하기 전에 별도로 검토하세요.
