# Stage 19 — 실제 RTX 3080 FE 헤드리스 언락 실험 (Ryzen 3600, 26:00.0)

**실제 GPU 레지스터 쓰기, DMA, FWSEC/SEC2 실행, PCIe FLR을 시도할 수 있는 실험용 이미지입니다.** 소스·PE 컴파일·정적 검사와 설치 실패 차단만 검사했으며, **이 새로운 Stage19 경로는 실제 하드웨어로 검증되지 않았습니다.** 기존 Stage 12의 성공이 현재 단독 GPU 구성에서의 성공·Windows 부팅을 보장하지 않습니다. GPU가 응답하지 않거나 화면 출력 경로가 복구되지 않으면 CPU 측 프로그램도 정지하고 Windows 부팅/로그 생성이 불가능합니다. 재부팅 후 복구에 다른 영상 출력 장치가 필요할 수 있습니다.

## 대상 환경 및 사전조건

- USB에서 Stage15 부트 브리지(`BOOTX64.EFI`, SHA-256 `e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00`)로 정상 Windows 진입한 상태.
- GPU: NVIDIA RTX 3080 FE `10de:2206`, subsystem `1467:10de`, BDF `0000:26:00.0`. 드라이버 분리 전 PCI Command `0x7`, PCI+GOP 드라이버 2개, PCI 자식 1개, NVIDIA GOP 하나.
- Stage18 `RTX3080-UNLOCK.EFI` SHA `4be34c5acdff24113a3cfab14e00f7b5438591d0392af9238aacc924da46342b`이 현재 USB에 있음.
- USB의 `RTX3080-UNLOCK/ASSETS` 아래에 **과거 Stage12 실험에서 사용한 것과 정확하게 동일한** 5개 바이너리 파일이 있음. `install-once.sh`가 5개 파일의 크기와 SHA-256을 검증하며 부족하면 설치를 거부함.
- USB와 USB에 쓰인 로그 이외에 중요한 자료 백업 권장. Secure Boot 비활성화 상태에서만 Stage12 엔진이 동작하도록 유지.

## 설치 (WSL)

```bash
cd ~/code/rtx3080-fe-reference/rtx3080-unlock-public-main
unzip -o '/mnt/c/Users/pc/Downloads/rtx3080-fe-stage19-experimental-headless-unlock.zip' -d .
bash fe/stage19/tests/run.sh
bash fe/stage19/install-once.sh /mnt/f
sync
```

`PASS: Stage19 EXPERIMENTAL unlock armed ONCE.`가 나오면 USB를 한 번만 UEFI 부팅 메뉴에서 선택. 실험 중 화면이 꺼질 수 있으며 **GPU/PC가 정지한 경우 무리하게 재시도하지 말 것**.

### 실행 구조

1. Stage15가 Windows Boot Manager를 로드한 후 일회성 마커를 제거하고 Stage19 브리지를 로딩
2. Stage19는 장치 ID와 기존 Stage17 소유권 구성을 검증하고 NVIDIA GOP/PCI 드라이버 `DisconnectController()` 수행
3. PCI Command `0x0`, UEFI 프로토콜 활성 드라이버 점유 없음 확인
4. `EFI_PCI_IO_PROTOCOL.Attributes()`로 **Memory Decode만** 켜 `Command=0x2`, BME=0인 상태 확인
5. 실제 Stage12 코드 기반 헤드리스 엔진 EFI를 로딩. BIOS 콘솔 입력을 요구하지 않고, 단독 GPU의 **드라이버가 이미 분리된 상태만 허용**, GPU 펌웨어 검증 후 원래의 DMA/FWSEC/SEC2/FLR 로직 실행
6. 엔진에서 `EFI_SUCCESS` 반환 시 `SS0=0x88888888`, `SS1=0x8` 읽기 검증
7. 분리 상태 PCI MSE를 되돌리고 `ConnectController()`로 GOP 재연결, Stage17과 같은 PCI/GOP 드라이버 관계 검증
8. 오류를 정상적으로 반환하면 Stage15는 Windows로 인계. **펌웨어가 정지하면 코드가 반환되지 않아 Windows 복귀 불가**.

**중요:** Stage12의 원래 `GuSafeToReturn` 및 GPU 상태 변경 후 오류 시 `Halt()` 정책을 유지함. 위험을 무시하고 복구 불확실한 GPU 상태에서 Windows로 넘어가게 하는 패치는 적용하지 않음. Stage19는 원래 성공했던 Stage12 코드의 동일한 펌웨어 자산 SHA-256과 PROM SHA-256 검증을 수행함. 현재 환경에서 읽힌 새 SS0 초기값 `0x10341014`는 Stage18에서 실제 관찰됐으나, 다른 ColdCheck 필드는 여전히 원본 게이트로 검증됨.

## 로그

- USB 루트: `RTX3080-STAGE15-YYYYMMDDHHMMSS.TXT` — 모듈 반환 및 Windows 인계 결과
- USB 루트: `RTX3080-STAGE19-UNLOCK.TXT` — 드라이버 분리/복귀, 엔진 실행, SS0/SS1 검증
- USB: `RTX3080-UNLOCK/RUNxx/REPORT.TXT` — Stage12 엔진 자체 단계별 상세 결과

## 복구

USB 없이 Windows 부팅 가능한 경우 WSL에서

```bash
bash fe/stage19/restore-stage18.sh /mnt/f
```

복구 스크립트는 `EFI/BOOT/RTX3080-UNLOCK.EFI`만 검증된 이전 Stage18 바이너리로 되돌리고, 일회성 마커를 지움. Stage15 `BOOTX64.EFI`는 건드리지 않음. USB 부팅을 건너뛰고 Windows가 정상으로 실행되면 이후 GPU 벤치마크를 비교할 것. 전원 완전 차단 후 기본 상태로 재설정되는 언락이므로 전원 껐다 켜면 재적용이 필요한 구조임.

## 빌드/정적검증

본 패키지의 EFI 두 개는 clang + lld-link PE32+ x64 UEFI 용으로 빌드됨. 소스 검사를 위해 `fe/stage19/engine/`에 실험용 Stage12 파생 소스 포함. `build-engine.sh`와 `build-bridge.sh`를 수동 실행하면 다시 빌드할 수 있음. 설치기는 제공 바이너리의 **정확한 SHA-256**를 검사해 다른 빌드는 거부함.
