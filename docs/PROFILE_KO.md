# RTX 3080 FE 수정 프로파일 — Stage 21 및 Stage 26

이 문서는 **2026-10-22 Stage 21 성공 실험의 프로파일**과 **2026-10-23 공유된 Stage 26 실기 결과**를 구분해 정리합니다. 원본 `rtx3080-unlock-public-main`의 MSI GPU 대상 포트를 NVIDIA RTX 3080 FE 전용으로 실험적으로 확장한 코드입니다. 모든 숫자는 **다른 메인보드에서도 같은 값을 써도 된다는 뜻이 아닙니다.**

## 1. 장치 식별 / PCI

| 조건 | 성공한 시스템의 값 | 주로 사용되는 코드 |
|---|---|---|
| PCI vendor / device | `10DE:2206` (`config[0] == 0x220610de`) | `fe/stage20/src/bridge.c`, `fe/stage21/engine/app.c`, `checks.c`, `port.h` |
| Founders Edition subsystem | `10DE:1467` (`config[0x2c/4] == 0x146710de`) | `fe/stage20/src/bridge.c`, `fe/stage21/engine/profiles.h` |
| PCI 도메인:버스:장치.기능 | `0000:26:00.0` | Stage20 `seg/bus/dev/fun`, Stage21 `Headless` 체크 |
| 기존 PCI Command | `0x7` (I/O+MSE+BME) | Stage20 초기 가드 |
| UEFI 드라이버 분리 뒤 | `0x0` | Stage20 분리 후 검사 |
| 언락 엔진에 건네기 전 | `0x2` (MSE ON, BME OFF) | Stage20 `Attributes()` |
| 엔진 내부 DMA 시 | `0x6`, 엔진 복귀 시 `0x2` | `fe/stage21/engine` |
| BAR0 | `0xFB000000`, 크기 `0x1000000` | Stage20 MMIO 진단, Stage21 BAR 검사 |
| GPU CHIP ID | `0xb72000a1` | `fe/stage21/engine/fe_cold.h` |

`0:38:0.0`으로 찍힌 내부 REPORT 로그는 **버스를 10진수 38로 출력**한 것으로, 외부 `26:00.0`과 같은 값입니다(`0x26 = 38`).

## 2. GOP 및 UEFI 소유권

- 내장 GPU가 없는 Ryzen 5 3600. NVIDIA RTX 3080 한 장에 UEFI GOP 1개.
- PCI Driver Binding 소유자 1개, PCI Child Controller 1개, GOP Driver Binding 소유자 1개 등 **Stage17/18에서 관측한 점유 관계**만 승인.
- `DisconnectController(gpu,NULL,NULL)` 후 PCI/GOP Driver Binding 점유가 0임을 확인.
- 언락에 성공한 뒤에는 **`ConnectController(gpu,NULL,NULL,TRUE)` 호출을 생략**하고, Stage15에서 메모리에 선적재한 Windows Boot Manager로 인계.
- 실제 Windows NVIDIA 드라이버에서 영상 출력이 재개되는지 확인해야 성공입니다.

코드: `fe/stage20/src/bridge.c`, `fe/stage15/src/bootbridge.c`.

## 3. 콜드 상태 판단 (Stage 21의 과거 기준 / Stage 26의 현재 실험 정책)

코드: `fe/stage21/engine/fe_cold.h`, `fe/stage21/engine/app.c`.

- 기본 GPU 판정: `chip == 0xb72000a1`.
- `GFW_PLM == 0x8b8f`, `GFW_BOOT == 0x3ff`, `GFW_CPUCTL == 0x10`.
- `WPR2_HI == 0`.
- `FEAT_PLM == 0xffffff8f`.
- Stage 21 과거 허용 SS0: `0x10240014`, `0x10241014`, `0x10341014`, `0x10140014`. **Stage 26에서 이 목록은 기록·비교용일 뿐 실행 허가 조건이 아닙니다.**
- 콜드 초기 SS1: `0x7`.
- `plm_1183a4 == 0x2800`.
- `booter_fuse == 0x1`, `fwsec_fuse == 0x3`.
- `gfw_idle`, `signature_slots_supported` 등 기존 게이트 유지.

**Stage 26 현재 실험 정책:** `fe_cold.h`의 `environment_ok_without_ss0`에서 초기 SS0의 전체값 화이트리스트를 제거했습니다. `SS0=0xffffffff`는 MMIO 오류 센티넬로 거부하며, Chip/GFW/WPR2/FEAT_PLM/SS1/0x1183a4/Fuse 등 나머지 조건을 유지합니다. `app.c`는 `STAGE26 SS0 snapshot`으로 원본값과 `legacy_whitelist` 통계값을 모두 남깁니다.

**실측:** `0x50340010`에서도 `stage20_selector_verify=1` 및 Windows FP16/BF16 성능 증가를 확인했습니다. 이는 **한 번의 성공 사례**일 뿐, 임의 SS0에서 실행하는 것이 안전함을 보장하지 않습니다. 세부 근거는 [Stage 26 결과](STAGE26_RESULTS_KO.md)를 참고하세요.

## 4. 실제 언락 레지스터 / 검증 기준

| 항목 | 기존 관측 | 언락 트랜잭션 후 |
|---|---|---|
| SS0 (`0x0082381C`) | `0x10140014`(Stage 21), `0x50340010`(Stage 26 실측) | **`0x88888888`** |
| SS1 (`0x00823820`) | `0x00000007` | **`0x00000008`** |
| FEAT PLM (`0x00823804`) | `0xffffff8f` | 언락 펌웨어 경로의 별도 검증 대상 |
| PCIe FLR | 실행 전 유휴 상태 | `result=0`, BAR 재접근·드라이버 응답 점검 |

실제 REPORT에는 `transaction result=0`, `FLR=1`, `FINAL status=0 clean=1`이 기록됐고, Stage20 브리지에서는 SS0/SS1 읽기와 `stage20_selector_verify=1`을 확인했습니다. **레지스터 쓰기 성공만으로 Windows 성능을 보장하지 않으므로**, 동일 벤치마크로 FP16/BF16 향상을 재확인해야 합니다.

## 5. GPU 펌웨어 입력 — GitHub에는 해시만 공개

Stage12 성공 때 사용한 NVIDIA 610.43.03 계열 자산과 정확히 동일한 조합:

| 파일 | 바이트 | SHA-256 |
|---|---:|---|
| `booter_load_ga102.bin` | 61,256 | `056cbc064a888f80717726c54d7f64fcd68044d123870c95a74f37f4975dcaa4` |
| `v67-ga102-610.43.03.bin` | 64,000 | `2a0aa1f86d0b6afff5214d641d87d46dc98ef06ed908ff05d0ddbad5ffb7e197` |
| `gsp_ga10x.bin` | 84,277,400 | `572373620a37418f24dc16b5031c39338778c3257e48e8408de9a57291b24f3a` |
| `gsp_boot_ga102.bin` | 24,576 | `3df651acb174fd5f5561a1996965b192f50c5d763168446ed0d4ead19d895768` |
| `gsp_boot_desc_ga102.bin` | 84 | `cc5a3fc1f4c47f2fe107b1b68f0176d8a448e6dec84b575e5560ccef5262bf15` |

실행 당시 **USB 경로는** `RTX3080-UNLOCK/ASSETS/`입니다. 소유권/저작권 문제가 있는 GPU 펌웨어 원본은 배포하지 마세요.

## 6. FE 프로파일 소스 차이

- `fe/stage21/engine/profiles.h`: FE(`0x146710de`)를 `GU_PROFILE_FE_OBSERVED`로 식별. 파일 주석의 초기 설명은 기존 프로파일이 FE 언락을 차단하던 시점의 기록입니다. **승인 조건은 이 파일 하나가 아니라** Stage20 브리지, `app.c`, `checks.c`, `port.c` 등의 가드 전체에 흩어져 있습니다.
- `fe/stage21/engine/app.c`: GPU 장치 BDF와 PCI/GOP 상태를 확인한 뒤 인터랙티브 입력 없이 트랜잭션 진입. 기존 보드 프로파일만 보고 승인하지 않음.
- `fe/stage21/engine/fe_cold.h`: Stage 21 과거 허용값과 Stage 26 현재 기록 전용 정책을 구분해 구현.
- `fe/stage20/src/bridge.c`: 대상 RTX 3080 FE의 PCI BDF, 드라이버 점유 구조 검증 → 분리 → MMIO 활성 → 엔진 호출 → **엔진 실행 후 GOP 재연결 생략**.
- `fe/stage15/src/bootbridge.c`: Windows 선적재, 한 번만 실행되는 마커 소비 및 실패 반환 기록.

**GPU 제조사·VBIOS·메인보드가 달라졌다면 이 값들을 복사해 승인 처리하지 말고, 읽기 전용 진단 단계부터 다시 검증하세요.**
