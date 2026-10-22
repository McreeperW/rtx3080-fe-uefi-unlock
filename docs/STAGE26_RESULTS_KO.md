# RTX 3080 FE Stage 26 — 초기 SS0 화이트리스트 밖 실기 성공 기록

**결과 공유일:** 2026-10-23 (KST). **실제 실행 시각:** 원본 타임스탬프 미제공. **검증 범위:** RTX 3080 FE 한 장, 한 번의 UEFI 언락/Windows 벤치마크 성공 사례.

## 시험 목표

Stage 21의 초기 SS0 전체값 허용 목록 때문에 콜드 상태 검사가 종종 `EFI_NOT_READY`를 반환했습니다. Stage 26에서는 초기 SS0를 읽고 로그에 기록하되 `known_observation`을 허용/거부 게이트로 쓰지 않습니다. `0xffffffff`의 MMIO 오류 센티넬을 거부하고 칩/GFW/WPR2/PLM/SS1/퓨즈 및 UEFI 소유권/펌웨어 자산 검사, 실패 시 재시도 잠금 등 기존 조건은 유지합니다.

## 실기 결과

- 장치: RTX 3080 Founders Edition `10de:2206`, subsystem `10de:1467`; 단독 디스플레이 GPU; Ryzen 5 3600.
- 초기 SS0/SS1: `0x50340010` / `0x7`. 과거 네 값의 SS0 화이트리스트에 없음 (`legacy_whitelist=0`).
- `cold-state`, `armed-cold-state`, `prom`, `assets`, `arming`, `armed-config`, `armed-display`: 모두 `status=0`.
- 엔진 `transaction result=0`, `FLR=1`, `FINAL status=0 ... attempted=1 ... clean=1`.
- Stage20에서 `engine_return_status=0`, 최종 SS0/SS1 `0x88888888` / `0x8`, `selector_verify=1`.
- Stage20의 로그 문구 `WINDOWS_UNVERIFIED`는 **UEFI 기록 시점의 상태**이며, 이후의 별도 Windows 벤치마크 실행이 성공하면서 실제 Windows 인계와 CUDA 연산이 가능했음을 확인했습니다.

| 연산 | BEFORE (TFLOPS) | AFTER (TFLOPS) | 차이 |
|---|---:|---:|---:|
| FP32 IEEE | 22.965 | 23.684 | +3.13% |
| FP16 Tensor | 62.924 | 116.156 | **+84.60%** |
| BF16 Tensor | 62.939 | 117.016 | **+85.92%** |

명령: `py .\benchmark.py --label after`, GEMM `n=4096`, 연산 유형별 `samples=30`.

## 증거 및 재현 범위

- [`evidence/stage26/selector-excerpt.txt`](../evidence/stage26/selector-excerpt.txt): 사용자가 제공한 **Stage20 실제 파일**의 선별된 원문 라인. 원본 파일 SHA-256은 파일 첫머리에 기록했습니다. 원본의 기존 실행 잔여/중복 라인은 발췌에서 제외했습니다.
- [`evidence/stage26/engine-excerpt.txt`](../evidence/stage26/engine-excerpt.txt): 채팅에 제공된 **Stage26 엔진 PowerShell 요약**을 재기록한 것으로, 원본 전체 엔진 로그 파일은 이번 업로드에 포함되지 않았습니다.
- [`results/stage26-benchmark-summary.json`](../results/stage26-benchmark-summary.json): 채팅에서 보고된 `benchmark.py` 실측 출력의 구조화된 기록입니다. 원본 `RTX3080-after.json` 및 벤치마크 실행 소스는 이 추가 업데이트에 포함되지 않았습니다.

## 해석과 제한

1. **이 부팅에서 초기 SS0가 기존 허용 목록 밖이어도 UEFI 트랜잭션과 Windows FP16/BF16 성능 향상이 성공한 것은 확인되었습니다.**
2. 초기 SS0가 어떤 값이든 항상 안전하거나 성공한다는 결론은 낼 수 없습니다. 다수 콜드부팅, 재시작, GPU 부하·온도 안정성, 실패 후 자동 복구는 검증되지 않았습니다.
3. FP32 +3.13%는 측정 오차·클럭·온도·전력 상태 등 다른 요인과 구별하지 않았으며, **FP32 연산 유닛이 언락됐다는 근거가 아닙니다**.
4. 빌드/단위 테스트와 실기 테스트는 별도로 표기합니다. GPU VBIOS 플래시나 영구 변경은 검증 범위가 아닙니다.
5. 비공개 저장소로 운용 중이라면 이 자료 추가가 저장소의 공개 범위를 바꾸지 않습니다. 펌웨어 바이너리, 개인 로그 전체, 복제된 부트 이미지 등은 공개하지 않습니다.
