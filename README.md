# RTX 3080 FE UEFI Tensor Unlock

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
