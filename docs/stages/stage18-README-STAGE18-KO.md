# Stage 18: 분리 직후 PCI 및 GPU 잠금 상태 읽기 전용 검증

**Stage17 결과**: `DisconnectController=EFI_SUCCESS`, `released_owner_bits=0`, `ConnectController=EFI_SUCCESS`, GOP 재발견 1개. 이것은 GPU 펌웨어 언락과 FLR 이후 복구를 의미하지 않습니다.

**이번 버전**은 Stage17의 검증된 UEFI 드라이버 분리/재연결 사이에 PCI 설정과 MMIO 레지스터를 **읽기만** 합니다. 언락 트랜잭션을 실행하지 않고 NVIDIA 펌웨어/VBIOS/PCI 레지스터에 직접 쓰지 않습니다. `DisconnectController()`와 `ConnectController()`는 드라이버를 통해 장치 상태에 영향을 줄 수 있으며 검은 화면·정지 위험이 있습니다.

## 검증 대상

- 기존 PCI Command `0x7`이 드라이버 해제 뒤 어떻게 변하는지 (`stage18_released_command...`). 이전 Stage12 실험의 초기값은 `0x2`.
- BAR0 `0xFB000000`, Bus Master `0x4` bit, Memory Decode `0x2` bit 확인.
- 메모리 디코드 활성화와 정확한 BAR가 확인된 경우에만 `0x0` CHIP, `0x823804` PLM, `0x82381C` SS0, `0x823820` SS1을 **읽기 전용**으로 조회.
- 그 뒤 UEFI 드라이버 재연결 및 GOP 복원 확인.

## 설치

Stage17이 설치된 USB를 Windows/WSL에 연결하고:

```bash
cd ~/code/rtx3080-fe-reference/rtx3080-unlock-public-main
unzip -o "/mnt/c/Users/pc/Downloads/rtx3080-fe-stage18-released-state.zip" -d .
bash fe/stage18/tests/run.sh
bash fe/stage18/install-once.sh /mnt/f
sync
```

**필수**: USB의 Stage15 `BOOTX64.EFI` SHA256 `e322ab5332fd420af93a7f61f94502df9cefbbfffc3cfbab37aa43f718451d00` 및 Stage17 `RTX3080-UNLOCK.EFI` SHA256 `1870c3fd372a86bd7f272339abff22d1fd81ac541b8dec0042766aea7ccde021` 일치, 일회성 마커 없음, 새로운 Stage17 백업 경로 비어 있음. 설치 시 Stage17을 `EFI/BOOT/RTX3080-STAGE17-ARCHIVE.EFI`로 백업하고 Stage18 child로 교체합니다. 기존 Windows SSD의 EFI 파티션은 건드리지 않습니다.

## 실행 및 결과

USB에서 UEFI 일회성 부팅을 1회 진행한 뒤 다음 파일을 업로드하세요.

- `F:\RTX3080-STAGE18-SNAPSHOT.TXT`
- 새로운 `F:\RTX3080-STAGE15-*.TXT`

Stage18은 GPU 소유권이 해제되지 않았거나 PCI protocol read가 실패하면 쓰기 없이 건너뛰고 가능한 한 드라이버 재연결을 시도합니다. EFI child가 반환하면 Stage15는 Windows로 인계합니다. 펌웨어가 정지하거나 디스플레이가 복귀하지 않는 경우에는 자동 Windows 진입이나 실패 로그가 보장되지 않습니다. USB를 제거하고 수동으로 원래 경로로 부팅해야 할 수 있습니다.

부팅 전 취소 및 Stage17 복원:

```bash
bash fe/stage18/restore-stage17.sh /mnt/f
sync
```

## 빌드/테스트 범위

- clang/lld-link로 UEFI PE32+ x64 컴파일
- 코드의 직접 PCI/MMIO 쓰기 미포함 검사
- 임시 FAT 구조에서 설치·무결성·1회 마커·복원 테스트
- **실제 Stage18 RTX3080 FE 하드웨어 실행 및 언락은 미검증**
