# RTX 3080 FE UEFI Tensor Unlock (Stage 21)

> **실험용 소스 / 검증 대상은 NVIDIA GeForce RTX 3080 Founders Edition 한 장입니다.** 일반 사용자용 드라이버·VBIOS 패치가 아닙니다. GPU 펌웨어 실행과 PCIe FLR로 화면 출력 및 시스템 부팅이 중단될 수 있습니다. 다른 컴퓨터에서 무작정 실행하지 마세요.

## 프로젝트 개요

Windows NVIDIA 드라이버가 GPU를 초기화하기 전에 UEFI 단계에서 RTX 3080 FE의 Tensor 관련 설정을 변경하고, **NVIDIA GOP를 재연결하지 않은 채 Windows Boot Manager로 인계**하는 실험입니다. GPU VBIOS는 플래시하지 않습니다. 현재 구현은 1회 실행 마커 방식이며, 전원을 끈 뒤 언락이 영구 유지되도록 만든 것은 아닙니다.

검증된 조합은 **Stage 15 부팅 브리지 + Stage 20 무-GOP-재연결 브리지 + Stage 21 언락 엔진**입니다. Stage 19에서 언락 자체는 성공했지만 GOP 재연결 과정으로 추정되는 정지가 있었고, Stage 20만 실행한 경우에는 콜드 상태 검사에서 거부되었습니다. Stage 21에서 해당 상태를 제한적으로 수용해 Windows 진입 및 Tensor 성능 향상을 측정했습니다.

## 실제 테스트 PC

| 구분 | 실측 또는 대화에서 확인된 내용 |
|---|---|
| GPU | NVIDIA RTX 3080 Founders Edition, 10 GiB, `VEN_10DE&DEV_2206`, subsystem `10DE:1467` |
| CPU | AMD Ryzen 5 3600 (내장 GPU 없음) |
| 메인보드 | MSI B450M Mortar Max (기존 PC 정보 기준, Stage 21 부팅 로그에서 모델명 자체를 재확인한 것은 아님) |
| 부팅 환경 | x64 UEFI, USB FAT32, Windows / WSL을 이용해 제작·실행 |
| PCI BDF | `0000:26:00.0` (GPU 로그의 `0:38:0.0`은 버스 `0x26`의 10진수 표기) |
| PCI config | 연결 전 Command `0x0007`; UEFI 드라이버 분리 후 `0x0000`; MMIO 활성 시 `0x0002` |
| BAR0 | `0xFB000000`, 16 MiB MMIO 범위 확인 |
| Windows 그래픽 드라이버 | NVIDIA 610.88, WDDM, CUDA UMD 13.3 |
| 참고 VBIOS | `94.02.27.00.13` (이전 조사에서 확인한 값) |
| GPU 펌웨어 입력 | NVIDIA 610.43.03 계열에서 준비한 자산 5종, 해시 고정 (자산 자체는 미포함) |

## 확인된 성능 (2026-10-22, 행렬 크기 4096, 각 30회 표본)

| 항목 | 언락 전 | Stage 21 적용 후 | 변화 |
|---|---:|---:|---:|
| FP32 IEEE | 22.965 TFLOPS | 22.385 TFLOPS | −2.53% |
| FP16 Tensor | 62.924 TFLOPS | 117.067 TFLOPS | **+86.05%** |
| BF16 Tensor | 62.939 TFLOPS | 118.045 TFLOPS | **+87.55%** |

이 값은 **해당 벤치마크 워크로드의 실제 측정값**입니다. 모든 AI 작업에서 동일한 속도 향상을 보장하지 않습니다. 벤치마크 원본 `benchmark.py`는 이 자료에 포함되지 않아, 결과 요약만 `results/stage21-benchmark-summary.json`에 보존했습니다.

## 실행 구조

```text
UEFI USB: EFI/BOOT/BOOTX64.EFI                # Stage 15, Windows Boot Manager 선적재
  └── EFI/BOOT/RTX3080-UNLOCK.EFI           # Stage 20, GPU 드라이버 분리
       └── EFI/BOOT/RTX3080-STAGE19-ENGINE.EFI  # 실제 내용은 Stage 21 엔진
            ├── 하드웨어·펌웨어·콜드 상태 검증
            ├── SEC2/GSP / DMA / FWSEC / PCIe FLR
            └── SS0=0x88888888, SS1=0x8 검증
       └── 성공 후 UEFI NVIDIA GOP 재연결 생략
  └── 선적재한 Windows Boot Manager 실행
       └── Windows NVIDIA 드라이버 인식 및 FP16/BF16 벤치마크
```

**중요:** Stage 21 엔진 파일명이 여전히 `RTX3080-STAGE19-ENGINE.EFI`인 것은 Stage 20 브리지가 해당 파일명을 사용하기 때문입니다. 이름이 Stage 19라는 이유로 이전 바이너리와 혼동하지 마세요.

## 저장소 구조

- `fe/stage12`~`fe/stage21` — 단계별 소스·빌드/설치/복원 스크립트. 이전 시행착오 재현을 위한 기록이며 **모든 단계가 동일하게 성공한 것은 아닙니다**.
- `unlock/`, `deps/edk2/`, `LICENSES/`, `THIRD_PARTY_NOTICES.md` — 기존 GPU unlock 포트와 빌드에 필요한 코드·헤더·원저작물 고지.
- `docs/PROFILE_KO.md` — **RTX 3080 FE용 수정 프로파일과 안전 검사값**.
- `docs/PORTING_AM4_KO.md` — **다른 AM4 메인보드/CPU로 옮길 때 수정·검증해야 하는 지점**.
- `docs/BUILD_AND_DEPLOY_KO.md` — 빌드, 바이너리 해시, USB, 복구 주의사항.
- `docs/GITHUB_KO.md` — GitHub 업로드 방법.
- `docs/stages/` — 단계별 원래 설명과 실험 이력.
- `windows/` — Windows에서 실패 로그를 여는 선택적 스크립트.

## 소스 빌드 (실제 실행 전 추가 검증 필수)

WSL/Linux에 `clang`, `lld-link`, `python3`, `gcc/cc`, EDK2 `MdePkg/Include`가 필요합니다. 아래는 **코드를 빌드하는 작업이며 USB 설치/실행은 하지 않습니다.**

```bash
bash fe/stage15/build.sh
bash fe/stage20/build.sh
bash fe/stage21/build-engine.sh
```

**이 공개 저장소는 소스 전용입니다.** 생성된 `*.EFI`, `*.obj` 및 NVIDIA 펌웨어 `*.bin`은 들어 있지 않습니다. 당시 검증된 EFI 해시는 `docs/BUILD_AND_DEPLOY_KO.md`에 별도 기록했습니다. 특히 Stage21의 일부 vendor 코드에는 컴파일 시 절대 소스 경로(`__FILE__`)가 포함되어 **다른 경로에서 빌드하면 SHA-256이 달라질 수 있습니다**. 따라서 고정 해시 검사를 임의로 제거하고 설치하면 안 됩니다. 단계별 설치 스크립트는 당시 생성한 **정확한 검증 바이너리와 이전 설치 상태**를 가정하며, 새 PC에서 이 저장소만 클론했다고 바로 실행할 수 있는 범용 설치기가 아닙니다.

## 다른 AM4 CPU·메인보드에서도 되나요?

**검증되지 않았습니다.** Ryzen 5600/5700X/5800X3D 등 같은 플랫폼에서도 PCI BDF, UEFI GOP/PCI 드라이버 점유, BAR 리소스, 초기 SS0·PLM·GFW·WPR2 상태가 달라질 수 있습니다. `0x26`으로 고정한 PCI 버스 확인을 제거하거나 `known_observation` 체크를 우회하는 것만으로 안전해지지 않습니다. 변경할 소스 위치와 단계별 검사 방법은 **[AM4 이식 안내](docs/PORTING_AM4_KO.md)**를 참고하세요.

## 위험 및 복구

- GPU 장치 제어와 FLR 중 **화면 신호 상실, 펌웨어 정지, 시스템 강제 종료, 드라이버 오류, 데이터 손실** 가능성이 있습니다. 모든 복구가 자동으로 되지 않습니다.
- Stage 15의 `RTX3080-STAGE15-ARM.ONCE`는 **실행 전에 소비**되므로 일반적인 실패 뒤 무제한 재실행하지 않도록 되어 있습니다. 이 파일을 임의로 다시 만들지 마세요.
- `stage15_windows_handoff=1`은 Windows 실행 직전 도달의 뜻입니다. Windows 부팅 성공과 GPU 언락 성공은 별도 확인해야 합니다.
- 언락 상태를 검증하려면 `nvidia-smi` 및 **언락 전/후 동일한 FP16/BF16 벤치마크**가 필요합니다.
- 복구 시 먼저 **USB 제거 후 PC의 기존 Windows 부팅**으로 돌아오세요. 검증되지 않은 EFI를 내장 SSD EFI 시스템 파티션에 바로 설치하지 마세요.
- Secure Boot, BitLocker 및 UEFI 보안 설정은 사용자 환경마다 다릅니다. Secure Boot 비활성화는 보안상 영향을 주므로 복구·보안 계획 없이 설정을 바꾸지 마세요.
- **펌웨어나 GPU 상태가 불확실할 때 원본의 무한정지 보호 루프를 임의 제거하는 것은 안전한 해결책이 아닙니다.**

## 라이선스·출처

파생 코드에 포함된 GPU unlock 코어의 라이선스는 **GPL-2.0-only**이며, 기존 출처·저작권 고지를 그대로 보존했습니다. 주요 출처는 [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock)과 [TianoCore/edk2](https://github.com/tianocore/edk2)입니다. 자세한 내용은 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md), [`LICENSE`](LICENSE) 및 `LICENSES/`를 확인하세요. **NVIDIA 드라이버·GSP/FWSEC 펌웨어 바이너리는 재배포하지 않습니다.**

> **재현 범위:** 이 저장소의 최종 검증 결과는 위 한 대의 PC와 당시 Windows 드라이버/펌웨어 구성에서 나온 것입니다. 다른 보드·다른 GPU·다른 드라이버에서 즉시 사용할 수 있다는 주장은 하지 않습니다.
