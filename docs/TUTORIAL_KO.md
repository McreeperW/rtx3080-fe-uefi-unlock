# RTX 3080 FE Stage 21 — USB 부팅 튜토리얼 (한국어)

> **연구용/고위험, 범용 설치기가 아닙니다.** 이 문서는 검증된 1대의 PC 구성과 **이미 정확한 펌웨어 자산 및 검증 바이너리를 확보한 경우**의 USB 제작 흐름을 설명합니다. GPU DMA, NVIDIA FWSEC·SEC2/GSP 실행, PCIe FLR 때문에 검은 화면, 시스템 정지, 강제 전원 차단 및 데이터 손실이 발생할 수 있습니다. UEFI/Windows 복구가 보장되지 않습니다. 다른 CPU·메인보드·GPU는 아래 명령을 그대로 실행하지 마세요.

**빠른 이동:** [준비물](#1-준비물) · [소스 빌드](#2-3개-efi를-한-번에-빌드) · [펌웨어 ASSETS](#3-펌웨어-assets-5개-준비) · [USB 제작](#4-fat32-usb-제작) · [부팅](#5-usb로-1회-부팅) · [결과 확인](#6-windows-로그와-성능-확인) · [실패·복구](#7-실패했을-때--재시도-주의)

## 0. 이 프로젝트의 구조와 검증 범위

한 번의 성공 실험에서 USB로 로드한 세 EFI 파일은 **개별 모듈**입니다. 소스는 서로 나뉘어 있지만, 사용자에게는 `bash scripts/build-all.sh`라는 통합 빌드 명령을 제공합니다.

| USB 경로 | 빌드 소스 | 역할 |
|---|---|---|
| `EFI/BOOT/BOOTX64.EFI` | `fe/stage15` | UEFI 부팅 및 Windows Boot Manager 선적재·실행 |
| `EFI/BOOT/RTX3080-UNLOCK.EFI` | `fe/stage20` | RTX 3080 PCI 드라이버 분리, 엔진 실행 및 GOP 재연결 생략 |
| `EFI/BOOT/RTX3080-STAGE19-ENGINE.EFI` | `fe/stage21` | 실제 FWSEC·SEC2/GSP·FLR 언락 엔진 |

**파일명에 `STAGE19`가 포함돼도 마지막 파일의 내용은 반드시 Stage 21 엔진이어야 합니다.** Stage 18·19 EFI 백업은 과거의 순차 업그레이드용 설치 스크립트에 필요한 파일일 뿐, **위 세 파일을 새 USB에 배치해서 부팅하는 런타임에서는 요구하지 않습니다**. 과거 `fe/stage20/install-once.sh`, `fe/stage21/install-once.sh`는 특정 기존 USB 상태와 과거 바이너리 해시를 가정하므로 새 USB에 순서대로 실행하는 범용 설치기가 **아닙니다**.

실제 성공 환경: RTX 3080 FE `10DE:2206`, Subsystem `10DE:1467`, BDF `0000:26:00.0`, Ryzen 5 3600, NVIDIA Windows 드라이버 610.88, 단독 GPU GOP. 메인보드는 기존 사용자 정보상 MSI B450M Mortar Max(부팅 로그에서 모델명을 독립 확인한 것은 아님). Ryzen 5000/B550/X570도 미검증입니다. 별도 [하드웨어 이식 가이드](PORTING_AM4_KO.md)와 [GPU 프로파일](PROFILE_KO.md)을 먼저 확인하세요.

## 1. 준비물

- **FAT32 USB** 1개(1 GiB 이상 권장). 중요한 파일은 백업하고, **Windows OS가 설치된 디스크의 EFI 시스템 파티션(ESP)은 절대 작업 대상으로 삼지 않습니다**.
- Windows 10/11 UEFI 부팅 PC + WSL2/Linux(예: Ubuntu). `git`, `clang`, `lld-link`, `python3`, C 컴파일러(`cc`), `sha256sum`, `unzip`.
- 본 저장소의 Stage 15·20·21 **전체 소스와 포함된 `deps/edk2/MdePkg` 헤더**.
- **NVIDIA 610.43.03 기반 ASSETS 5개**. 이 저장소는 원본/파생 NVIDIA 펌웨어 바이너리를 제공하지 않고, 검증된 신규 추출기를 제공하지도 않습니다. [ASSETS 상세 문서](ASSETS_GUIDE_KO.md) 확인.
- 세 EFI 파일을 **성공 실험과 동일한 SHA-256**으로 확보한 상태. Stage 21은 일부 `__FILE__` 진단 문자열에 컴파일 당시 절대 경로가 포함되어 **다른 경로에서 빌드하면 해시가 달라질 수 있습니다**. 동일한 소스라는 이유만으로 실행 검증된 파일이 되지 않습니다.
- 문제가 생겼을 때 USB를 제거하고 기존 Windows로 돌아갈 복구 수단. BitLocker 사용 시 복구 키·UEFI 설정 변경 영향을 미리 확인하세요.

> **주의:** 위 자산/EFI 동일성이 확보되지 않았다면 **여기서 멈추세요**. 파일명만 바꾸거나 해시 검사를 우회하는 것은 해결책이 아닙니다. 공개 저장소만 클론해 누구나 그대로 실행하는 배포 상태는 아직 아닙니다.

## 2. 3개 EFI를 한 번에 빌드

WSL Ubuntu 예시:

```bash
sudo apt update
sudo apt install -y git clang lld python3 gcc unzip coreutils

git clone https://github.com/<GITHUB-USER>/rtx3080-fe-uefi-unlock.git
cd rtx3080-fe-uefi-unlock

# 콜드 상태 CPU 단위 테스트 + Stage15/20/21 소스 빌드
bash scripts/build-all.sh
```

`<GITHUB-USER>`는 실제 저장소 소유자 이름으로 바꾸세요. 이미 `git clone`했다면 프로젝트 폴더에서 마지막 명령만 실행하면 됩니다. **이 명령은 빌드만 하며 USB에 쓰거나 GPU를 언락하지 않습니다.**

생성되는 출력물:

```text
fe/stage15/out/BOOTX64.EFI
fe/stage20/out/RTX3080-UNLOCK.EFI
fe/stage21/out/RTX3080-STAGE19-ENGINE.EFI
```

각 파일의 SHA-256을 기존 성공 빌드와 비교해 `MATCH` 또는 `DIFF / NOT TESTED`를 표시합니다. **Clang 빌드 성공 = GPU 호환성/안전성 증명이 아닙니다.** 특히 Stage 21의 해시 불일치는 이미 보고된 재현성 한계입니다. 최신 변경으로 바이너리가 달라졌다면 실기 검증 전에는 USB를 실행 대상으로 준비하지 마세요.

## 3. 펌웨어 ASSETS 5개 준비

정확히 다음 다섯 파일이 필요합니다. `gsp_ga10x.bin`만 있거나, Windows 드라이버에 포함된 이름이 비슷한 파일로 대체해서는 안 됩니다.

| 파일명 | 크기 (바이트) | SHA-256 |
|---|---:|---|
| `booter_load_ga102.bin` | 61,256 | `056cbc064a888f80717726c54d7f64fcd68044d123870c95a74f37f4975dcaa4` |
| `v67-ga102-610.43.03.bin` | 64,000 | `2a0aa1f86d0b6afff5214d641d87d46dc98ef06ed908ff05d0ddbad5ffb7e197` |
| `gsp_ga10x.bin` | 84,277,400 | `572373620a37418f24dc16b5031c39338778c3257e48e8408de9a57291b24f3a` |
| `gsp_boot_ga102.bin` | 24,576 | `3df651acb174fd5f5561a1996965b192f50c5d763168446ed0d4ead19d895768` |
| `gsp_boot_desc_ga102.bin` | 84 | `cc5a3fc1f4c47f2fe107b1b68f0176d8a448e6dec84b575e5560ccef5262bf15` |

자산은 본인이 합법적으로 확보한 파일이어야 합니다. 원본 NVIDIA 드라이버의 어떤 파일을 어떻게 가공했는지에 관한 **끝까지 검증된 공개 생성 절차는 아직 없습니다**. 5개 자산이 없다면 GPU 실행 단계로 넘어가지 마세요.

파일을 `~/gpu-assets/`에 준비한 경우, 준비된 **빌드 EFI + 펌웨어**를 다음과 같이 사전 검사합니다.

```bash
# 프로젝트 최상위 폴더에서
python3 scripts/check-usb.py --built --assets-dir "$HOME/gpu-assets"
```

**모든 EFI·ASSETS가 성공 기록의 해시와 일치해야 `PASS`**를 출력합니다. 파일이 없다면 먼저 확보해야 합니다. 준비된 3개 *검증 EFI*를 다른 폴더에 모았다면 대신 다음처럼 확인할 수 있습니다.

```bash
python3 scripts/check-usb.py --efi-dir "$HOME/verified-efi" --assets-dir "$HOME/gpu-assets"
```

## 4. FAT32 USB 제작

먼저 Windows **PowerShell**에서 USB의 드라이브 문자를 확인합니다. 다음 예시는 USB가 **F:**인 경우만 해당합니다.

```powershell
Get-Volume -DriveLetter F | Select-Object DriveLetter, FileSystem, DriveType, Size
```

F:가 실제 작업할 별도 USB인지 확인하고, FAT32가 아니면 **필요한 데이터를 백업한 뒤** Windows 디스크 관리 등에서 FAT32로 준비하세요. 이 문서는 자동 포맷하지 않습니다. **내장 SSD의 EFI 파티션을 사용하지 마세요.**

WSL에서 USB를 `/mnt/f`에 마운트(이미 자동 마운트됐다면 건너뜀):

```bash
sudo mkdir -p /mnt/f
mountpoint -q /mnt/f || sudo mount -t drvfs F: /mnt/f
```

USB를 **빈 부팅용 USB로 새로 준비한 경우** 다음처럼 복사합니다. `BOOTX64.EFI`가 이미 존재하면 덮어쓰지 말고 USB 대상이 맞는지 먼저 확인하세요.

```bash
# WSL / 프로젝트 루트 / F:가 실제 FAT32 USB라는 사실을 확인한 뒤에만 실행
(
  set -e
  USB=/mnt/f
  ROOT="$PWD"
  ASSETS="$HOME/gpu-assets"  # 위 단계에서 전부 SHA-256 검증된 파일 5개

  # 준비된 로컬 빌드 EFI와 펌웨어의 해시가 틀리면 중단
  python3 scripts/check-usb.py --built --assets-dir "$ASSETS"

  # 기존 EFI 부팅 파일이 있으면 절대 덮어쓰지 않음
  if [ -e "$USB/EFI/BOOT/BOOTX64.EFI" ]; then
    echo 'STOP: 이 USB에는 이미 BOOTX64.EFI가 있습니다. 덮어쓰지 마세요.'
    exit 1
  fi

  mkdir -p "$USB/EFI/BOOT" "$USB/RTX3080-UNLOCK/ASSETS"
  cp "$ROOT/fe/stage15/out/BOOTX64.EFI" "$USB/EFI/BOOT/"
  cp "$ROOT/fe/stage20/out/RTX3080-UNLOCK.EFI" "$USB/EFI/BOOT/"
  cp "$ROOT/fe/stage21/out/RTX3080-STAGE19-ENGINE.EFI" "$USB/EFI/BOOT/"
  cp "$ASSETS"/*.bin "$USB/RTX3080-UNLOCK/ASSETS/"
  sync

  # USB 복사 후에도 크기·SHA-256 재검사 (아직 GPU 실행 허가 마커 없음)
  python3 scripts/check-usb.py --usb "$USB"
)
```

※ 이 복사 예시는 세 **로컬 빌드 EFI의 해시가 성공 사례와 일치하는 경우에만** 실행됩니다. Stage 21 빌드 해시가 달라 실패한다면 중단하세요. 원래 테스트 PC에서 백업한 세 **검증 EFI 파일**을 가진 경우에는 별도 폴더에 모아 `--efi-dir`로 동일성을 확인한 후 USB에 배치할 수 있으나, 그 파일이 다른 메인보드에서도 안전하다는 의미는 아닙니다.

USB의 최종 구조:

```text
F:\
├── EFI\
│   └── BOOT\
│       ├── BOOTX64.EFI                   ← Stage 15
│       ├── RTX3080-UNLOCK.EFI            ← Stage 20
│       └── RTX3080-STAGE19-ENGINE.EFI    ← 내용은 Stage 21
└── RTX3080-UNLOCK\
    └── ASSETS\
        ├── booter_load_ga102.bin
        ├── v67-ga102-610.43.03.bin
        ├── gsp_ga10x.bin
        ├── gsp_boot_ga102.bin
        └── gsp_boot_desc_ga102.bin
```

## 5. USB 부팅 (1회 실행 / 자동 모드)

기본은 1회 실행입니다. **이미 성공한 동일 PC·USB에서 매번 자동 언락하려면** [자동 USB 모드](AUTO_USB_KO.md)를 참고하세요. 자동 모드는 새로운 Stage 15 바이너리이므로 실기 미검증입니다.

### 기본 1회 실행

1. 중요 작업을 저장하고, USB를 뽑은 상태에서도 Windows가 정상적으로 부팅되는지 확인합니다.
2. **검증된 RTX 3080 FE와 동일한 보드/UEFI 토폴로지**이며 위험을 이해하고 진행할 때만 다음 명령으로 1회 실행 마커를 생성합니다. 이것은 의도적인 **GPU 펌웨어 쓰기/FLR 실험 허가**입니다.

```bash
# 직전 검증 통과, F:가 실제 전용 USB임을 최종 확인한 뒤에만
python3 scripts/check-usb.py --usb /mnt/f
printf 'I_ACCEPT_GPU_HANG_RISK' > /mnt/f/RTX3080-STAGE15-ARM.ONCE
sync
python3 scripts/check-usb.py --usb /mnt/f --require-arm
```

3. Windows 재시작 후 **메인보드의 UEFI 1회 부팅 메뉴**(제조사에 따라 F11/F12/Esc 등)를 열어 USB의 **UEFI 부팅 항목**을 선택합니다. 부팅 순서를 영구 변경할 필요는 없습니다.
4. Stage 15는 Windows Boot Manager를 먼저 적재하고 Stage 20→Stage 21을 실행합니다. UEFI 드라이버 분리·펌웨어 트랜잭션 중 일시적으로 영상 출력이 사라질 수 있습니다. **Windows 인계가 보장되는 것은 아니며** 무한 정지 시 자동 복구도 불가능합니다.
5. Stage 15는 실행 전에 `RTX3080-STAGE15-ARM.ONCE`를 소비하므로 **같은 USB로 다시 부팅한다고 언락을 자동 반복하지 않습니다**. 문제가 발생한 뒤 임의로 마커를 복구하거나 반복 시도하지 마세요.

Secure Boot 등 UEFI 정책 때문에 서명되지 않은 EFI 부팅이 거부될 수 있습니다. 다른 시스템에서 확인 없이 보안 기능을 비활성화하는 것을 일반 설치 절차로 안내하지 않습니다.

## 6. Windows 로그와 성능 확인

Windows에 진입했다면 PowerShell에서 다음을 확인합니다.

```powershell
nvidia-smi
Get-Content F:\RTX3080-STAGE20-UNLOCK.TXT -Tail 35
Get-ChildItem F:\RTX3080-UNLOCK -Recurse -Filter REPORT.TXT |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 3 FullName,LastWriteTime
Get-ChildItem F:\RTX3080-STAGE15-*.TXT |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 3 FullName,LastWriteTime
```

성공 사례에서 확인된 **세 단계별** 기록은 다음과 같습니다.

```text
# Stage21 엔진: RTX3080-UNLOCK/RUNxx/REPORT.TXT
post-FLR SEC2 reset released ... SS0=88888888 SS1=00000008
FINAL status=0 stage=transaction attempted=1 driver_release=0 clean=1

# Stage20 호출 브리지
stage19_engine_return_status=0x0000000000000000
stage19_post_engine_ss0=0x0000000088888888
stage19_post_engine_ss1=0x0000000000000008
stage20_selector_verify=0x0000000000000001
stage20_result=PASS_UNLOCK_SELECTORS_SKIP_GOP_RECONNECT_WINDOWS_UNVERIFIED

# Stage15 부팅 브리지
stage15_child_startimage_return=0x0000000000000000
stage15_result=CHILD_RETURNED_SUCCESS_UNLOCK_UNVERIFIED
stage15_windows_handoff=1
```

**로그의 `PASS`만으로 Windows NVIDIA 드라이버 이후 언락 성능 유지가 증명되지는 않습니다.** `nvidia-smi`에서 GPU/드라이버 응답을 확인하고, **언락 전/후 정확히 같은 FP16/BF16 행렬 벤치마크**로 판단하세요. 검증 PC의 최종 재측정 결과는 FP16 **62.924 → 118.567 TFLOPS (+88.43%)**, BF16 **62.939 → 120.699 TFLOPS (+91.77%)**입니다. 기존 첫 AFTER 측정은 1860 MHz 언더볼팅 설정이었습니다. 이 수치는 해당 실험 워크로드의 결과이지 모든 AI 처리량이 그만큼 증가한다는 뜻이 아닙니다.

## 7. 실패했을 때 / 재시도 주의

- **검은 화면·응답 없음:** 무리한 반복 실행을 피하고, 필요할 경우 전원 차단 후 **USB를 제거한 상태**에서 기존 Windows 부팅 경로로 복구합니다. 강제 종료는 마지막 수단이며 미저장 데이터가 손실될 수 있습니다.
- Stage 19에서 언락 트랜잭션 자체는 성공했지만 UEFI GOP 재연결 과정에서 화면이 돌아오지 않은 이력이 있습니다. Stage 20/21의 재연결 생략도 **다른 UEFI에서 성공이 보장되지 않습니다**.
- USB를 다시 연결한 뒤에는 `RTX3080-STAGE20-UNLOCK.TXT`, 최신 `RTX3080-STAGE15-*.TXT`, 최신 `RTX3080-UNLOCK/RUNxx/REPORT.TXT`를 보존합니다. `FAILED_CHILD_RETURNED`, `EFI_NOT_READY`, 정지 직전 기록의 의미를 구분하세요.
- Stage 21 최초 실험 후 **전원 완전 차단 시 언락 설정은 다시 초기화될 것으로 예상**됩니다. 재부팅·절전 유지 여부는 별도 검증 대상입니다. 이 저장소는 매번 자동 적용되거나 VBIOS를 영구 수정하는 제품이 아닙니다.
- 복구된 뒤에는 Windows ESP나 VBIOS가 아닌 **USB에만** 변경을 한정하세요. 디스크 EFI 파티션에 설치하거나 1회 실행 제한을 제거하기 전에 안전한 복구 루트를 별도로 검증해야 합니다.

## 8. 배포 상태와 기여 포인트

지금의 저장소는 **Stage 21 성공 사례와 전체 소스가 공개된 연구 저장소**이며, 범용 GPU 언락 프로그램(원클릭 설치기)은 아닙니다. 다음 항목이 해결돼야 일반 사용자가 처음부터 독립 재현하기 쉬워집니다.

1. NVIDIA 펌웨어 5개 파일의 합법적인 획득·추출·생성 과정을 **버전·해시 포함 완전 재현**.
2. Stage 21의 `__FILE__` 절대 경로를 포함한 **재현 가능한 바이너리 빌드**와 그 결과에 대한 실기 검증.
3. PCI `26:00.0` 하드코딩을 동적 대상 탐색으로 바꾸되 GPU 소유권·GOP·BAR·초기 상태 검증을 보존.
4. 다른 보드/CPU에서 **읽기 전용 진단 → 조건 비교 → 단일 실험** 순으로 검증하고, 실패 복귀 경로를 확인.

이 제약은 [GPU 프로파일](PROFILE_KO.md), [AM4 이식 안내](PORTING_AM4_KO.md), [기존 빌드 문서](BUILD_AND_DEPLOY_KO.md)에 기록돼 있습니다.
