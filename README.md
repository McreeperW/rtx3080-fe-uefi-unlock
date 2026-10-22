# RTX 3080 FE UEFI Tensor Unlock

<!-- STAGE26_BUILD_GUIDE: BEGIN -->
## Stage 21 / Stage 26: 완전히 분리된 빌드

두 버전은 **각각 독립된 `engine/` 소스**를 사용합니다. Stage 21은 원래의 초기 SS0 네 값 화이트리스트를 유지하고, Stage 26은 초기 SS0를 기록만 하며 다른 GFW/PLM/WPR2/SS1 등 검사를 유지합니다.

```bash
# Stage 21: 기존 SS0 화이트리스트 정책
bash fe/stage21/build-engine.sh
bash fe/stage21/tests/run.sh

# Stage 26: SS0 관측 전용 정책, 독립된 엔진 및 검증
bash fe/stage26/build.sh
```

| 버전 | 소스 | 빌드 출력 (USB 설치 아님) |
|---|---|---|
| Stage 21 | `fe/stage21/engine/` | `fe/stage21/out/RTX3080-STAGE19-ENGINE.EFI` |
| Stage 26 | `fe/stage26/engine/` | `fe/stage26/out/RTX3080-STAGE19-ENGINE.EFI` 및 구별용 `RTX3080-STAGE26-ENGINE.EFI` |

**중요:** Stage 20이 USB에서 요청하는 엔진 파일명은 버전에 상관없이 `EFI/BOOT/RTX3080-STAGE19-ENGINE.EFI`입니다. 두 `out/` 폴더의 파일은 서로 덮어쓰지 않습니다. 직접 복사해 USB에 설치하거나 자동 실행 마커를 활성화하는 기능은 포함되지 않습니다. 소스 빌드 성공과 UEFI 실기 안전성은 별개입니다.

[Stage 26 빌드 절차](docs/STAGE26_BUILD_KO.md) · [Stage 26 실기 결과](docs/STAGE26_RESULTS_KO.md) · [두 정책 비교](docs/STAGE21_STAGE26_COMPARISON_KO.md)
<!-- STAGE26_BUILD_GUIDE: END -->


RTX 3080 Founders Edition용 **실험적 UEFI Tensor 언락**.
Windows 부팅 전 GPU 설정을 변경합니다. VBIOS 플래시는 하지 않습니다.

## 테스트 환경

- **GPU:** RTX 3080 FE 10GB (`10DE:2206`, `10DE:1467`)
- **CPU:** Ryzen 5 3600
- **메인보드:** MSI B450M Mortar Max
- **PCI:** `0000:26:00.0`
- **OS:** Windows / UEFI x64
- **NVIDIA 드라이버:** 610.88

## 성능

| 항목 | 언락 전 | 언락 후 | 향상 |
|---|---:|---:|---:|
| FP16 Tensor | 62.924 | 118.567 | **+88.43%** |
| BF16 Tensor | 62.939 | 120.699 | **+91.77%** |
| FP32 | 22.965 | 22.756 | −0.91% |

단위: TFLOPS · 4096×4096 · 30회 측정. 언락 전 순정 상태 기준.

## 사용

1. [설치·USB 부팅 튜토리얼](docs/TUTORIAL_KO.md)
2. [필수 ASSETS 5개](docs/ASSETS_GUIDE_KO.md)
3. [GPU 프로파일](docs/PROFILE_KO.md) · [다른 AM4 시스템 이식](docs/PORTING_AM4_KO.md)

통합 빌드:

```bash
bash scripts/build-all.sh
```

**ASSETS 5개 생성:**

```bash
git clone https://github.com/Zagitta/gpu-unlock.git ~/gpu-unlock
make -C ~/gpu-unlock assets
python3 scripts/verify-assets.py ~/.cache/gpu-unlock/assets
```

생성된 5개 파일을 USB `RTX3080-UNLOCK/ASSETS/`에 복사. 해시 불일치 시 실행하지 마세요. [자세히](docs/ASSETS_GUIDE_KO.md).

**USB 구성 (FAT32)**

```text
USB/
├── EFI/BOOT/
│   ├── BOOTX64.EFI                 # Stage 15
│   ├── RTX3080-UNLOCK.EFI          # Stage 20
│   └── RTX3080-STAGE19-ENGINE.EFI  # Stage 21
├── RTX3080-UNLOCK/ASSETS/
│   ├── booter_load_ga102.bin
│   ├── v67-ga102-610.43.03.bin
│   ├── gsp_ga10x.bin
│   ├── gsp_boot_ga102.bin
│   └── gsp_boot_desc_ga102.bin
└── RTX3080-STAGE15-ARM.ONCE        # 1회 실행 (기본)
```

EFI 3개는 빌드, ASSETS 5개는 별도 준비. 파일 검사 후 USB로 부팅하면 Windows로 인계합니다.

**USB 부팅마다 자동 언락 (선택):** [자동 모드 안내](docs/AUTO_USB_KO.md)

```bash
bash fe/stage15/build.sh
bash scripts/install-auto-usb.sh /mnt/f
```

자동 모드에서는 USB 루트에 `RTX3080-STAGE15-AUTO.ON`이 유지됩니다.

## 주의

- **RTX 3080 FE 단일 시스템에서만 검증.** 다른 메인보드·CPU·GPU는 미검증.
- GPU 정지, 검은 화면, 강제 종료 위험. 실패 시 USB 제거 후 일반 부팅.
- 언락은 영구 적용되지 않음. 기본 1회 실행 / 실험적 USB 자동 실행 선택 가능.
- NVIDIA 펌웨어 ASSETS와 빌드된 EFI는 저장소에 포함되지 않음.

## 라이선스

GPL-2.0-only. 원저작물 고지: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Stage 26 — SS0 화이트리스트 제외 실기 검증

RTX 3080 FE (10DE:2206)에서 초기 SS0=0x50340010,
SS1=0x7 상태로 UEFI 언락에 성공했습니다.

기존 Stage 21의 초기 SS0 전체값 화이트리스트는
Stage 26에서 측정·기록 전용으로 변경했습니다.
기존 GPU 식별, GFW, WPR2, PLM, Fuse 및
펌웨어 자산 검사는 유지합니다.

언락 후 SS0=0x88888888, SS1=0x8,
Stage 20 selector_verify=1, 엔진 transaction result=0,
FLR=1을 확인했습니다.

| 연산 | 이전 | 이후 | 변화 |
|---|---:|---:|---:|
| FP32 IEEE | 22.965 | 23.684 TFLOPS | +3.13% |
| FP16 Tensor | 62.924 | 116.156 TFLOPS | +84.60% |
| BF16 Tensor | 62.939 | 117.016 TFLOPS | +85.92% |

행렬 크기 4096, 각 30개 표본입니다.

이 결과는 단일 실기 성공 사례입니다. 반복 콜드부팅
성공률과 장시간 안정성은 아직 검증되지 않았습니다.
FP32 증가만으로 FP32 언락을 주장하지 않습니다.

[Stage 26 상세 검증 결과](docs/STAGE26_RESULTS_KO.md)
