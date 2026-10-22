# Stage 21 / Stage 26 정책 및 테스트 분리

| 구분 | Stage 21 | Stage 26 |
|---|---|---|
| 소스 | `fe/stage21/engine/` | `fe/stage26/engine/` (독립 복사) |
| 실험 정책 | SS0 전체값 4개 고정 허용 | 초기 SS0 값 로깅, 전체값 화이트리스트로 거부하지 않음 |
| SS0 후보 | `0x10240014`, `0x10241014`, `0x10341014`, `0x10140014` | 그 외 값도 다른 콜드 상태 검사가 통과하면 진행할 수 있음 |
| 안전 조건 | GFW idle, PLM, WPR2, Fuse, SS1 등 | 동일 항목 유지, `0xffffffff` 읽기 오류 센티넬 거부 |
| 빌드 | `bash fe/stage21/build-engine.sh` | `bash fe/stage26/build.sh` |
| 단위 시험 | `bash fe/stage21/tests/run.sh` | Stage26 빌드 안에서 `fe/stage26/tests/run.sh` 수행 |
| 출력 디렉터리 | `fe/stage21/out` | `fe/stage26/out` |

Stage21 소스 복원 기준은 기존 `rtx3080-fe-stage21-guarded-coldstate.zip`의 `engine/app.c`와 `engine/fe_cold.h`입니다. Stage26은 당시 실기 성공한 Stage26 소스를 분리해 보존합니다. 이 두 소스 버전이 달라졌다는 사실이 **별도 실기 성공**을 뜻하지는 않습니다.

두 버전은 GPU 펌웨어 실행·FLR 등이 포함된 위험한 실험용 코드입니다. 이 리팩터링 패키지는 USB 설치, 실행, 재시도, UEFI 부팅 설정을 수행하지 않습니다. 과거 `install-once.sh`나 다른 오래된 스크립트를 자동 실행하지 마세요.
