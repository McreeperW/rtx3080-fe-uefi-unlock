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

USB에는 **Stage 15 부트로더, Stage 20 브리지, Stage 21 엔진**을 사용합니다. EFI 파일 배치와 검증 절차는 튜토리얼을 참고하세요.

## 주의

- **RTX 3080 FE 단일 시스템에서만 검증.** 다른 메인보드·CPU·GPU는 미검증.
- GPU 정지, 검은 화면, 강제 종료 위험. 실패 시 USB 제거 후 일반 부팅.
- 언락은 영구 적용되지 않음. 현재는 1회 실행 방식.
- NVIDIA 펌웨어 ASSETS와 빌드된 EFI는 저장소에 포함되지 않음.

## 라이선스

GPL-2.0-only. 원저작물 고지: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
