# 소스 빌드, 검증 바이너리 및 USB 취급 주의

<!-- STAGE26_BUILD_GUIDE: BEGIN -->
## Stage 26 (현재) 빌드 안내

이 문서의 기존 SHA-256 및 Stage 21 설치 스크립트 설명은 **과거 Stage 21 검증 파일**을 기준으로 합니다. 현재 Stage 26 SS0 기록 전용 정책을 빌드·검증하려면 다음 명령과 [Stage 26 빌드 가이드](STAGE26_BUILD_KO.md)를 사용하세요.

```bash
bash fe/stage26/build.sh
```

실제 엔진 파일명은 Stage 20 호출 호환성 때문에 `fe/stage21/out/RTX3080-STAGE19-ENGINE.EFI`로 유지됩니다. 새 빌드 파일의 해시는 이전 Stage 21 값과 다를 수 있으며, 과거 `install-once.sh`로 임의 설치하지 마세요.
<!-- STAGE26_BUILD_GUIDE: END -->

## 소스만 공개하는 이유

펌웨어 파일 5개를 어디에 배치하고 어떤 해시로 검증하는지는 [ASSETS 준비·검증 가이드](ASSETS_GUIDE_KO.md)에 별도로 정리했습니다. **자산 확보/추출의 완전 자동화는 아직 제공하지 않습니다.**

이 저장소는 전체 C 소스/스크립트와 단계별 문서를 제공하지만, **실험용 실행 EFI, NVIDIA 펌웨어/드라이버 바이너리 및 개인 하드웨어 로그는 커밋하지 않습니다.** 저장소를 `git clone`한 것만으로 완성된 Stage21 언락 USB가 생기지는 않습니다. 설치 스크립트는 특정 이전 파일들의 **정확한 SHA-256을 요구**하므로 새로운 컴파일 파일의 해시가 달라지면 안전하게 거부합니다. 이런 경우 기존 검증 파일을 무작정 덮어쓰거나 해시 검사를 끄지 마세요.

## 빌드 환경

- WSL2 Linux (Clang, LLD `lld-link`, GCC/CC, Python 3, GNU coreutils, `file`)
- 포함된 `deps/edk2/MdePkg/Include` (BSD-2-Clause-Patent) 및 GPU 포트 소스
- `bash fe/stage15/build.sh`, `bash fe/stage20/build.sh`, `bash fe/stage21/build-engine.sh`
- 결과는 각각 `fe/stage15/out/BOOTX64.EFI`, `fe/stage20/out/RTX3080-UNLOCK.EFI`, `fe/stage21/out/RTX3080-STAGE19-ENGINE.EFI`. 컴파일해도 **정확한 하드웨어 안전성을 보장하지 않음**.

### 빌드 재현성에서 확인된 차이

이 저장소에서 Stage15, Stage20 소스를 다시 빌드하면 테스트 당시의 SHA-256과 일치하는 것을 확인했습니다. **Stage21 엔진은 소스 빌드가 성공하더라도 과거 성공 바이너리와 SHA-256이 일치하지 않을 수 있습니다.** 당시 일부 vendor 소스의 `__FILE__` 문자열에 절대 빌드 경로가 포함되어 있었고, 재빌드 위치에 따라 바이너리 바이트가 달라집니다. 따라서 해시 불일치를 무시하고 `install-once.sh`의 제한을 삭제하지 마세요. 동일한 소스라는 이유만으로 새 바이너리가 실기 검증된 것은 아닙니다. 원래 성공한 Stage21 바이너리는 반드시 별도로 로컬 보관하세요.

소스 검사는 다음과 같이 실행할 수 있습니다(새 EFI를 USB에 설치하지 않음).

```bash
bash scripts/audit-sources.sh
bash scripts/test-stage21-cold.sh
```

## 기존 검증된 EFI SHA-256 (실험 당시)

| 구성 요소 | SHA-256 |
|---|---|
| Stage15 `BOOTX64.EFI` | `e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00` |
| Stage20 `RTX3080-UNLOCK.EFI` | `8ef922e0804bf5b0d68f028e8af2c77b676ebfb9b0b3307b7311516e8747d4c5` |
| Stage21 `RTX3080-STAGE19-ENGINE.EFI` | `0d578a0612fb8a59b7320a0e038d2f19b03f9573b2a995f10ed44e6e10b32373` |
| Stage18 백업 진단 EFI | `4be34c5acdff24113a3cfab14e00f7b5438591d0392af9238aacc924da46342b` |

위 해시가 맞는 것은 **파일 동일성**이고 다른 PC에서 안전하다는 뜻이 아닙니다.

## 당시 USB 파일 구조 (로컬에만 보관)

```text
USB:/
├── EFI/BOOT/BOOTX64.EFI                   # Stage15
├── EFI/BOOT/RTX3080-UNLOCK.EFI            # Stage20
├── EFI/BOOT/RTX3080-STAGE19-ENGINE.EFI     # 내용은 Stage21 엔진
├── RTX3080-UNLOCK/ASSETS/*.bin             # 펌웨어 5개 (Git 비공개)
└── RTX3080-STAGE15-ARM.ONCE                # 실행 직전에 소비되는 1회 마커
```

**안전 수칙:** 성공한 Stage21 실험 후에도 해당 마커를 임의로 복구해 매 부팅 강제 실행하지 마세요. Stage19에서 실제 GPU 펌웨어 트랜잭션은 성공했지만 GOP 재연결에 이르러 PC가 멈추는 이력이 있습니다.

## 로그 확인

- `RTX3080-STAGE20-UNLOCK.TXT`: `stage19_engine_return_status=0`, `stage20_selector_verify=1` 등
- `RTX3080-UNLOCK/RUNxx/REPORT.TXT`: `FINAL status=0 ... attempted=1 clean=1`
- `RTX3080-STAGE15-*.TXT`: 외부 EFI 성공 반환, Windows 인계 시도
- Windows `nvidia-smi`: 정상 RTX3080 장치 및 NVIDIA 드라이버 감지
- 정확히 동일한 FP16/BF16 벤치마크: 최종 성능 확인

## 복구 주의

- 오류 뒤 모니터가 검게 나오거나 GPU가 응답하지 않으면 먼저 USB를 제거하고 원래 부팅 경로로 Windows를 켜세요. UEFI에서 모듈이 반환하지 않으면 Windows로 자동 복귀하지 못합니다.
- VBIOS를 영구 수정하지 않았고 성공 사례에서 전원 차단 후 기본 상태로 돌아가는 동작을 관찰했습니다.
- `fe/stage21/restore-stage20.sh`는 Stage21 엔진을 이전 Stage19 바이너리로 바꾸고 마커를 제거하도록 설계된 스크립트이므로, **실제 로컬 USB 파일 상태와 백업 해시를 확인한 후** 실행하세요.
- 내장 SSD의 Windows ESP를 덮어쓰지 말고 별도 USB 검증을 우선하세요.
