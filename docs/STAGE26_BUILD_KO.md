# Stage 26 빌드·검증 안내 (현재 소스 기준)

> **중요:** Stage 26은 독립적인 새로운 펌웨어 엔진이 아니라 `fe/stage21/engine/`의 **초기 SS0 검사 정책을 변경한 소스 버전**입니다. 이전 Stage 21 튜토리얼만 따라 하면 현재 사용 중인 Stage 26이 실제로 포함됐는지 구별하기 어렵습니다. 이 문서는 컴파일과 모의 테스트에 한정됩니다. 빌드와 USB 설치·자동 언락 활성화는 별도 작업입니다.

## 1. Stage 26 소스가 적용됐는지 확인

WSL에서 저장소 루트로 이동합니다.

```bash
cd ~/code/rtx3080-fe-uefi-unlock

grep -n 'STAGE26 SS0 snapshot' fe/stage21/engine/app.c
grep -n 'environment_ok_without_ss0' fe/stage21/engine/fe_cold.h
ls -l fe/stage21/tests/run-stage26.sh
```

모두 존재해야 합니다. 없으면 이 저장소는 아직 **이전 Stage 21 정책**일 수 있으므로 Stage 26이라고 표기해 빌드하면 안 됩니다. 기존 Stage 26 소스 패치를 먼저 적용·검증해야 합니다.

## 2. Stage 26 엔진 컴파일 및 테스트

필수 도구: WSL/Linux, `clang`, `lld-link`, `file`, `python3`, `sha256sum`, C 컴파일러, 저장소의 `deps/edk2/MdePkg/Include`.

```bash
cd ~/code/rtx3080-fe-uefi-unlock

# 수정된 Stage 21 엔진 코드를 Stage 26 정책으로 컴파일하고
# Stage 26 모의 테스트 및 UEFI PE 형식 검사를 수행
bash fe/stage26/build.sh
```

기존 빌드 스크립트를 직접 호출해도 **같은 Stage 26 소스**를 컴파일합니다.

```bash
bash fe/stage21/build-engine.sh
bash fe/stage21/tests/run-stage26.sh
```

새 편의 스크립트는 세 단계를 처리합니다: Stage 21 하위 빌더 호출 → Stage 26 테스트 → 구별 가능한 이름의 사본 생성 및 해시 검사. GPU 펌웨어를 실행하지 않으며 USB 파일을 수정하지 않습니다.

## 3. 빌드 산출물과 Stage 20 호환성

| 경로 | 설명 |
|---|---|
| `fe/stage21/out/RTX3080-STAGE19-ENGINE.EFI` | **실제 Stage 26 정책으로 빌드된 실행 파일.** Stage 20 브리지가 찾는 파일명 |
| `fe/stage26/out/RTX3080-STAGE26-ENGINE.EFI` | 위 파일의 **동일 바이트 복사본**. 빌드 결과를 구별해서 보관하기 위한 이름 |

UEFI 단계의 호출 순서는 변하지 않습니다.

```text
EFI/BOOT/BOOTX64.EFI                  Stage 15
  -> EFI/BOOT/RTX3080-UNLOCK.EFI      Stage 20
  -> EFI/BOOT/RTX3080-STAGE19-ENGINE.EFI
       내용: Stage 26 정책이 적용된 Stage 21 엔진
  -> Windows Boot Manager
```

Stage 20 브리지는 여전히 `RTX3080-STAGE19-ENGINE.EFI`라는 이름을 요청합니다. **`RTX3080-STAGE26-ENGINE.EFI`로 USB상의 파일명을 바꾸면 안 됩니다.**

## 4. 무엇이 변경됐나

- `fe/stage21/engine/fe_cold.h`: 초기 SS0 정확 일치 목록은 `known_observation` 기록만을 위한 참고값. GPU/GFW/PLM/WPR2/Fuse/SS1 조건과 `SS0=0xffffffff` 오류 센티넬 차단은 유지.
- `fe/stage21/engine/app.c`: `STAGE26 SS0 snapshot` 로그 추가; 콜드 상태는 `environment_ok_without_ss0` 기준으로 판정.
- `fe/stage21/tests/run-stage26.sh`, `test_stage26_policy.c`: 변경 정책과 독립적인 상태 검사를 모의 검증.
- **Stage 15/20 EFI 바이너리를 수정하는 빌드가 아닙니다.** 과거 Stage 21 및 Stage 26 결과에 대한 설명은 `docs/STAGE26_RESULTS_KO.md`를 참고하세요(결과 문서가 없는 경우 아직 결과 패치 미적용).

## 5. 실기 검증과 구분

로컬 빌드·모의 테스트가 통과했다는 사실은 실제 펌웨어/FLR 작업의 안전성을 의미하지 않습니다. 실제 Stage 26 첫 성공 당시 기록은 초기 `SS0=0x50340010`, 적용 뒤 `SS0=0x88888888`, `SS1=0x8` 및 Windows FP16/BF16의 +84.60%/+85.92% 측정입니다. **단일 성공 사례만 있으며 반복 콜드부팅 안정성은 미검증**입니다.

이 문서는 기존 USB EFI 해시 검사나 자동 실행 보호를 우회하는 설치 절차를 제공하지 않습니다. 새로 빌드한 EFI는 경로와 `__FILE__` 문자열 등에 따라 과거 실기 검증 바이너리와 해시가 달라질 수 있습니다. **빌드 성공을 실기 검증 완료로 표시해서는 안 됩니다.**
