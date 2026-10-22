# 빌드 및 검증

## 기본 검사

Windows에서 README의 컴파일러와 Python을 설치한 뒤 저장소 루트에서 실행합니다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\test-all.ps1
```

기본 검사는 probe, port/Bus Master, preflight, runtime, PE 거부 검사와 USB 패키지 도구를 실행합니다. `evidence/boot-03/REPORT.TXT`를 preflight fixture로 사용합니다. GPU와 UEFI 서비스는 모의 환경입니다.

Python 패키지 검사만 실행하려면 다음 명령을 사용합니다.

```text
python -m unittest discover -s tools -p "test_*.py" -v
```

## 로컬 fixture를 사용하는 추가 검사

다음 파일을 준비한 뒤 `-WithLocalFixtures`를 지정합니다.

- `analysis/pinned-assets/`: `unlock/assets.json`의 5개 자산.
- `evidence/boot-03/PROM.BIN`: 1MiB, SHA256 `F3DC8F07B0B8C9856D192DF172DFA7F7F84AC17EF6EDFDB448AD7FA94EFD2789`.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\test-all.ps1 -WithLocalFixtures
```

transaction, SHA256, EFI transaction, 전체 앱과 FWSEC 순서 비교 검사가 추가됩니다. PROM fixture는 기록된 보드의 회귀 검사 입력입니다. 다른 카드의 캡처로 바꿔 같은 검증이라고 해석하지 않습니다.

## 소스와 패키지 해시

`SOURCE-MANIFEST.json`에는 공개 소스 파일 목록과 SHA256이 있습니다. USB 패키지 도구는 이 목록에 있는 파일만 `SOURCE.zip`에 넣고 빌드 입력·EFI·실행 자산을 검사합니다. 개인 작업 파일과 빌드 캐시는 패키지에 넣지 않습니다.

소스를 수정한 개발자는 새 파일을 Git에 추가하고 소스 manifest를 갱신한 뒤 다시 빌드·검사합니다.

```powershell
git add .
python .\tools\source_manifest.py
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\test-all.ps1
```

`tools/source_manifest.py`는 Git의 추적 파일을 기준으로 동작합니다. ZIP으로 받은 소스는 제공된 manifest를 사용해 그대로 빌드하고 패키징할 수 있습니다.

검사 결과는 `unlock/build/test-results.json`에 기록합니다. host/mock 통과와 실제 USB 부팅 결과는 별개입니다.


## Stage 26 (2026-10-23 공유 결과) — 초기 SS0 로깅 정책

현재 `fe/stage21/engine/`에 반영된 코드는 초기 SS0를 특정 네 값으로 제한하지 않는 Stage 26 실험판입니다. 다음 로컬 검사에는 **GPU 펌웨어 실행이 포함되지 않습니다**.

```bash
bash fe/stage21/build-engine.sh
bash fe/stage21/tests/run.sh  # Stage 26 유닛/정적/PE 검사 호출
```

- `test_stage26_policy.c`: 미리 관찰된 화이트리스트 밖 SS0 여섯 값을 허용하되, 독립적인 10개 상태 가드와 `0xffffffff` 센티넬 거부를 확인합니다.
- `run-stage26.sh`: 빌드된 UEFI PE 아키텍처를 점검하고 위 모의 테스트를 실행합니다.
- 실제 USB 실험: [Stage 26 결과](docs/STAGE26_RESULTS_KO.md). 실기 테스트/Windows 성능 관측은 호스트 모의 테스트와 구별합니다.
- 옛 Stage 21용 해시 고정 테스트는 Stage 26 빌드에 직접 사용할 수 없어 `run.sh`의 호출 대상을 바꾸었습니다. Stage 21 당시 상태는 Git 이력에 보존합니다.
