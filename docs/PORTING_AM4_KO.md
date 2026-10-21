# 다른 AM4 메인보드·CPU로 옮길 때 확인할 사항

**중요:** 현재 실행 버전은 `Ryzen 5 3600 + RTX 3080 FE + BDF 0000:26:00.0`에서만 검증됐습니다. 새 시스템에서는 바로 Stage 21을 실행하지 말고 Stage 14~18의 읽기 중심 검증을 먼저 수행해야 합니다. 이 문서는 **수정 위치와 검증 대상을 제시**하는 것이며, 다른 시스템에서 GPU 쓰기나 FLR의 안전을 보장하지 않습니다.

## 1. 변경 유형별 영향

| 변경 | 우선 확인할 것 | 예상 코드 영향 |
|---|---|---|
| 같은 보드에서 Ryzen 3600 → 5600/5700X/5800X3D 등 | BIOS 지원, PCI BDF, PCIe 세대·GOP 초기화, SS0 | 고정 BDF 또는 콜드 상태 가드 수정 가능성 |
| B450M → B450I/B550I/X570I 등 보드 교체 | USB UEFI 부트옵션, BDF, GOP 수, Child Controller, BAR, Secure Boot | Stage15/20/21 장치 검색 및 컨트롤러 점유 검사 |
| 5600G/5700G 등 iGPU 있는 CPU | iGPU on/off, Primary Display 선택, GOP 1개 조건, RTX 3080 실제 콘솔 점유 | 단독 GPU 가드가 달라짐. 현 바이너리는 그대로 승인되지 않을 수 있음 |
| PCIe 슬롯 또는 BIOS 옵션 변경 | BDF, BAR0, 링크 상태/FLR, REBAR 설정 | 주소·크기·다중 장치 검사 |
| FE가 아닌 RTX 3080 AIB | Subsystem ID, VBIOS/FWSEC 서명과 ROM 레이아웃, 펌웨어 리비전 | 단순 PCI ID 바꾸는 것으로 이식 불가 |
| 다른 NVIDIA 드라이버/펌웨어 | GSP 초기화, 성공 후 Windows 인식, 벤치마크 | 펌웨어 자산 해시 검증 및 드라이버 호환성 재시험 |

## 2. 코드에서 시스템 고정값을 찾는 곳

```bash
grep -RInE 'bus!=0x26|bus != 0x26|0x146710de|0x220610de|0x10140014|0xfb000000|MAX_GOP' fe/stage15 fe/stage20 fe/stage21
```

- **GPU 검색:** `fe/stage20/src/bridge.c`의 `seg != 0 || bus != 0x26U || dev != 0 || fun != 0` 조건은 현 보드에 고정. Stage21 `fe/stage21/engine/app.c`에도 `bus!=0x26` 확인이 있어 **둘 다** 수정 범위. 범용화할 때는 BDF 하드코딩보다 `10DE:2206` + subsystem + 정확히 한 장 탐지 + 예상된 리소스 및 드라이버 점유 검사로 수정하는 방향을 검토하세요.
- **FE 장치 ID / AIB 구분:** `fe/stage20/src/bridge.c`, `fe/stage21/engine/app.c`, `checks.c`, `profiles.h`, `port.h`. FE 아닌 GPU는 기존 검증 프로파일로 취급하면 안 됩니다.
- **UEFI GOP/소유권:** `fe/stage20/src/bridge.c`의 NVIDIA GOP 식별, PCI/GOP `OpenProtocolInformation()` 개수/비트 및 `DisconnectController()` 호출 구조. 펌웨어마다 Open Count나 Child Controller 구성은 달라질 수 있습니다. **임의로 숫자를 무시하면 위험합니다.**
- **PCI/MMIO:** Stage20의 PCI Command `0x7 → 0x0 → 0x2` 확인, BAR0, 지원 Attributes; Stage21 `fe_bar.h`, `fe_pcie.h`, `port.c`. BAR 주소는 런타임 확인값이며 다른 보드의 고정 주소로 가정하면 안 됩니다.
- **GPU 콜드 상태:** `fe/stage21/engine/fe_cold.h`, `app.c`. `0x10140014`는 실측 변형의 제한적 허용일 뿐 모든 AM4 CPU에 허용되는 값이 아닙니다.
- **Windows 인계:** `fe/stage15/src/bootbridge.c`. BootOrder 내 Windows Boot Manager 찾기, 매칭 ESP fallback, 실패 시 Windows로 인계하는 경로를 재검증해야 합니다.
- **설치 스크립트:** `fe/stage19/install-once.sh`, `fe/stage20/install-once.sh`, `fe/stage21/install-once.sh`는 기존 **정확한 EFI 해시와 설치 단계**를 강제. 수정 빌드는 해당 바이너리/시험 결과를 다시 확인해야 하며, 단순히 실패 가드를 제거해 실행하지 마세요.

## 3. 보드 변경 후 권장 검증 순서

1. USB 제거 상태에서 Windows/기본 NVIDIA 그래픽 출력 정상 동작 확인. Windows 부팅 복구 USB/다른 GPU를 준비.
2. 읽기 전용 Stage14/16에서 PCI BDF, vendor/device/subsystem, GOP 경로, Child Controller 관계 기록.
3. Stage15의 **언락 마커 없는 상태**로 Windows Boot Manager 선적재 및 정상 Windows 실행 확인.
4. Stage17에서 (실험 위험을 감수할 경우) 드라이버 분리·재연결 시험. 영상 출력이 멈출 수 있습니다.
5. Stage18에서 분리 직후 PCI Command/BAR 등 기록. MMIO가 비활성화됐다면 무리하게 접근하지 않음.
6. Stage21 프로파일 상이점을 검토하고, 기존 Stage12/Stage19의 펌웨어 서명·WPR2·GFW·DMA·FLR·복구 가드를 유지. 다중 GPU 구조는 별도 포팅 필요.
7. 최종 UEFI 트랜잭션 성공, Windows 드라이버 응답, **언락 전/후 동일 벤치마크**를 모두 확인한 후에만 보드 호환성을 주장.

## 4. 주의

- PCI BDF는 보드/BIOS/슬롯별로 다르게 할당될 수 있습니다. 이 코드는 아직 **범용 PCI 자동 검색 버전이 아닙니다**.
- CPU가 동일해도 보드 펌웨어·GOP 구현에 따라 결과는 달라집니다.
- Ryzen G CPU를 장착했다고 자동으로 기존 AMD GOP 기반 Stage12 성공 경로를 재현하는 것은 아닙니다.
- Stage19의 재연결 정지는 원인이 완전하게 증명된 것은 아니지만, Stage21에서 **재연결 생략 후 Windows가 정상 부팅**한 사실은 검증됐습니다.
- **VBIOS 플래시/펌웨어 보안 우회 조건의 무차별 제거/하드코딩은 권장하지 않습니다.**
