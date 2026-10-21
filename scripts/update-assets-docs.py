#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Update three Korean docs with the traced Zagitta/gpu-unlock assets path.

Only touches README.md, docs/ASSETS_GUIDE_KO.md and docs/TUTORIAL_KO.md.
No firmware download, USB writes, arming or GPU access.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
README = ROOT / 'README.md'
GUIDE = ROOT / 'docs' / 'ASSETS_GUIDE_KO.md'
TUTORIAL = ROOT / 'docs' / 'TUTORIAL_KO.md'

SHORT = '''**ASSETS 5개 생성:**

```bash
git clone https://github.com/Zagitta/gpu-unlock.git ~/gpu-unlock
make -C ~/gpu-unlock assets
python3 scripts/verify-assets.py ~/.cache/gpu-unlock/assets
```

생성된 5개 파일을 USB `RTX3080-UNLOCK/ASSETS/`에 복사. 해시 불일치 시 실행하지 마세요. [자세히](docs/ASSETS_GUIDE_KO.md).
'''

GUIDE_SECTION = '''## 3. ASSETS 생성 — 이전 실험 기록

2026-10-20 작업에서 [`Zagitta/gpu-unlock`](https://github.com/Zagitta/gpu-unlock)의 `Makefile`을 사용했습니다. 원본 입력 버전은 **610.43.03**입니다.

```bash
# WSL/Linux; 첫 실행 시
git clone https://github.com/Zagitta/gpu-unlock.git ~/gpu-unlock
make -C ~/gpu-unlock assets

# RTX 3080 FE 언락 저장소 루트에서 실행
python3 scripts/verify-assets.py ~/.cache/gpu-unlock/assets
```

`make assets`는 해당 저장소에서 다음 작업을 연결합니다.

- `firmware/fetch_artifacts.py`: `NVIDIA-kernel-module-source-610.43.03.tar.xz`, `nvidia-utils-610.43.03-1-x86_64_v3.pkg.tar.zst` 다운로드 및 입력 해시 확인
- `firmware/generate_v67.py`: `v67-ga102-610.43.03.bin` 생성
- `firmware/extract_boot_assets.py`: `booter_load_ga102.bin`, `gsp_ga10x.bin`, `gsp_boot_ga102.bin`, `gsp_boot_desc_ga102.bin` 추출

기본 출력은 `~/.cache/gpu-unlock/assets/`입니다. 위 생성 명령은 **ASSETS만 준비**하며 USB·GPU는 변경하지 않습니다.

**중요:** 과거 `Makefile` 경로는 확인했지만, 현재 upstream 소스로 재생성한 5개 파일이 당시 성공한 SHA-256과 같은지는 별도 확인하지 않았습니다. 최신 upstream에서 빌드 내용이 달라졌다면 `verify-assets.py` 검사를 통과하지 못할 수 있습니다. 파일명 변경이나 해시 검사 우회는 하지 마세요.

'''

GUIDE_END = '''## 6. 생성·검증 범위

- **확인됨:** 기존 `gpu-unlock` `Makefile`에서 `make assets` → NVIDIA 입력 다운로드 → V67 생성·부팅 자산 추출의 경로를 확인했습니다.
- **실행 필요:** 새 환경에서 재생성한 파일의 크기·SHA-256이 위 5개와 완전히 같은지 검사해야 합니다.
- **지원 범위:** 정확한 ASSETS 일치는 파일 동일성 검증일 뿐입니다. 다른 GPU·메인보드·CPU의 안전성이나 언락 성공을 보장하지 않습니다.
- NVIDIA 펌웨어 바이너리를 GitHub에 업로드하지 마세요.
'''

TUTORIAL_SECTION = '''## 3. 펌웨어 ASSETS 5개 준비

[Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock)에서 NVIDIA **610.43.03** 입력으로 ASSETS 5개를 생성합니다.

```bash
# WSL/Linux: 첫 준비 시
git clone https://github.com/Zagitta/gpu-unlock.git ~/gpu-unlock
make -C ~/gpu-unlock assets

# RTX 3080 FE 언락 저장소 루트에서 무결성 검사
python3 scripts/verify-assets.py ~/.cache/gpu-unlock/assets
python3 scripts/check-usb.py --built --assets-dir "$HOME/.cache/gpu-unlock/assets"
```

생성 위치: `~/.cache/gpu-unlock/assets/`. `make assets` 내부에서 NVIDIA 입력 패키지를 받고 `generate_v67.py` 및 `extract_boot_assets.py`로 파일을 준비합니다. 검증에 사용할 5개 파일명과 **정확한 SHA-256**은 [ASSETS 가이드](ASSETS_GUIDE_KO.md)를 확인하세요.

**해시가 일치하지 않으면 USB 실행을 준비하지 마세요.** `make assets`를 실행했다는 사실만으로 Stage 21 성공 당시와 같은 바이너리가 만들어졌다고 단정할 수 없습니다. 생성된 NVIDIA 펌웨어를 GitHub에 직접 포함하지 않습니다.

'''


def section_replace(s, heading_regex, next_heading_regex, replacement, file):
    p = re.search(r'(?m)^## ' + heading_regex + r'\s*$', s)
    if not p:
        raise ValueError(f'{file}: target section missing')
    n = re.search(r'(?m)^## ' + next_heading_regex, s[p.end():])
    if not n:
        raise ValueError(f'{file}: following section missing')
    start = p.start()
    end = p.end() + n.start()
    return s[:start] + replacement + s[end:]


def guide_text(text):
    text = section_replace(text, r'3\..*', r'4\..*', GUIDE_SECTION, GUIDE.name)
    text = re.sub(r'(?m)^> \*\*상태:.*$',
                  '> **상태:** 생성 명령은 이전 기록에서 확인됨. 새 생성물은 반드시 크기·SHA-256 검증.', text, count=1)
    text = text.replace('이 파일은 소스 코드만 빌드해서 자동으로 생기지 않습니다.',
                        '이 파일은 본 저장소의 `build-all.sh`로 생성되지 않으며, 별도 `gpu-unlock`의 `make assets`로 준비합니다.')
    text = text.replace('이 저장소에는 실험 당시의 추출 명령과 전체 재현 기록이 없습니다.',
                        '이전 실험의 `make assets` 생성 경로는 확인됐습니다.')
    text = text.replace('이 저장소에는 Windows Stage 21용으로 검증된, 5개 파일 자동 다운로드·추출 스크립트가 없습니다.',
                        '별도 `gpu-unlock` 저장소의 `make assets`를 사용할 수 있으나, 생성 결과는 반드시 고정 해시로 비교해야 합니다.')
    p = re.search(r'(?m)^## 6\..*$', text)
    if not p:
        raise ValueError('ASSETS guide section 6 not found')
    text = text[:p.start()] + GUIDE_END
    return text


def tutorial_text(text):
    text = section_replace(text, r'3\..*', r'4\..*', TUTORIAL_SECTION, TUTORIAL.name)
    text = text.replace('이 저장소는 원본/파생 NVIDIA 펌웨어 바이너리를 제공하지 않고, 검증된 신규 추출기를 제공하지도 않습니다.',
                        '이 저장소는 NVIDIA 펌웨어 바이너리를 제공하지 않습니다. 별도 `gpu-unlock`의 `make assets`로 생성 후 검증해야 합니다.')
    text = text.replace('**이미 정확한 펌웨어 자산 및 검증 바이너리를 확보한 경우**',
                        '**정확한 펌웨어 자산 및 검증 바이너리를 확보한 경우**')
    text = text.replace('ASSETS="$HOME/gpu-assets"', 'ASSETS="$HOME/.cache/gpu-unlock/assets"')
    text = text.replace('`~/gpu-assets/`', '`~/.cache/gpu-unlock/assets/`')
    text = text.replace('"$HOME/gpu-assets"', '"$HOME/.cache/gpu-unlock/assets"')
    return text


def readme_text(text):
    if 'make -C ~/gpu-unlock assets' in text:
        return text
    if 'bash scripts/build-all.sh' in text:
        match = re.search(r'(?m)^bash scripts/build-all\.sh\s*\n```\s*\n', text)
        if match:
            return text[:match.end()] + SHORT + '\n' + text[match.end():]
    m = re.search(r'(?m)^## (?:사용|소스 빌드.*|설치.*)\s*$', text)
    if not m:
        raise ValueError('README: no installation/build section')
    return text[:m.end()] + '\n\n' + SHORT + text[m.end():]


def main():
    try:
        for p in (README, GUIDE, TUTORIAL):
            if not p.is_file():
                raise ValueError(f'missing required documentation: {p}')
        sources = {p: p.read_text(encoding='utf-8') for p in (README, GUIDE, TUTORIAL)}
        result = {
            README: readme_text(sources[README]),
            GUIDE: guide_text(sources[GUIDE]),
            TUTORIAL: tutorial_text(sources[TUTORIAL]),
        }
    except (OSError, ValueError) as error:
        print(f'ERROR: no files modified: {error}', file=sys.stderr)
        return 1
    for p, updated in result.items():
        if updated != sources[p]:
            p.write_text(updated, encoding='utf-8')
            print('UPDATED:', p.relative_to(ROOT))
        else:
            print('UNCHANGED:', p.relative_to(ROOT))
    return 0


if __name__ == '__main__':
    sys.exit(main())
