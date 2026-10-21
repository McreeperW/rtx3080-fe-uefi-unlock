#!/usr/bin/env python3
"""Idempotently add a quickstart section to README, retaining existing benchmark edits."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'README.md'
s=p.read_text(encoding='utf-8')
needle='## 프로젝트 개요'
heading='## 처음 사용하는 사람: USB 튜토리얼'
if heading in s:
    print('Already linked:',p)
elif needle not in s:
    raise SystemExit('ERROR: README layout changed; add docs/TUTORIAL_KO.md link manually')
else:
    section=(
        '## 처음 사용하는 사람: USB 튜토리얼\n\n'
        '**[처음부터 따라 하는 한글 튜토리얼 (준비물 → 빌드 → ASSETS → FAT32 USB → 부팅 → 로그·복구)](docs/TUTORIAL_KO.md)**\n\n'
        '> 공개 저장소의 소스 빌드는 실기 검증과 다릅니다. Stage21 엔진 재빌드 해시, NVIDIA ASSETS 준비, 다른 메인보드/CPU 호환성 제한을 먼저 읽으세요.\n\n'
    )
    s=s.replace(needle,section+needle,1)
    p.write_text(s,encoding='utf-8')
    print('Added tutorial link:',p)
