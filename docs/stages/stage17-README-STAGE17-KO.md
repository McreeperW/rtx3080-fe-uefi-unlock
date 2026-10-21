# Stage 17 — RTX 3080 FE 단독 GPU UEFI Driver Binding 분리/재연결 검증

## 목적과 실제 변경 사항

이 단계는 Stage16 로그에서 확인된 구조(PCI `0000:26:00.0`, PCI `0x7`, NVIDIA GOP 1개, PCI BY_DRIVER/BY_CHILD, GOP BY_DRIVER)에 대해 **UEFI Driver Binding의 Stop/Start 복구를 시험**합니다. 이전 Ryzen 2400G+AMD GOP 환경에서 성공한 Stage12의 SEC2/GSP/FLR 언락 코드를 **아직 호출하지 않습니다**. 이 단계의 목표는 Ryzen 3600에 보조 화면 출력이 없는 상태에서 기존 UEFI 드라이버를 풀고 다시 붙일 수 있는지 확인하는 것입니다.

UEFI 명세의 `DisconnectController()`와 `ConnectController()`를 사용합니다. **개발자가 GPU 레지스터를 직접 기록하거나 FLR을 호출하지 않지만, 연결 해제 과정에서 펌웨어의 Driver Binding Stop()이 GPU 하드웨어에 영향을 줄 수 있습니다.** RTX 3080이 유일한 화면 출력 장치이므로 검은 화면, 그래픽 복귀 실패, 컴퓨터 정지 위험이 있습니다. GPU가 멈추면 Windows Boot Manager를 미리 적재해도 자동 Windows 부팅과 오류 로그 기록을 보장할 수 없습니다.

최초 실제 하드웨어 실험이므로 아래 명령은 **사용자가 명시적으로 USB를 선택해 1회 부팅**하는 경우에만 실행됩니다. 마커는 Stage15가 모듈 실행 전에 소모하므로 재부팅 반복 위험을 줄입니다.

## 설치

요구 조건: 앞서 검증된 정확한 Stage15 `BOOTX64.EFI`와 Stage16 읽기 전용 child가 USB에 있어야 하고, 기존 일회성 마커가 없어야 합니다. Stage16 테스트 직후라면 다음처럼 실행합니다.

```bash
cd ~/code/rtx3080-fe-reference/rtx3080-unlock-public-main
unzip -o "/mnt/c/Users/pc/Downloads/rtx3080-fe-stage17-reconnect-test.zip" -d .
bash fe/stage17/tests/run.sh
bash fe/stage17/install-once.sh /mnt/f
sync
```

설치 스크립트는 기존 Stage16 child를 `EFI/BOOT/RTX3080-STAGE16-ARCHIVE.EFI`로 백업하고 새 Stage17 child를 `EFI/BOOT/RTX3080-UNLOCK.EFI`에 배치합니다. 기존 Stage15 `BOOTX64.EFI`는 변경하지 않습니다. 이전에 WSL `drvfs`에서 오류가 발생했던 `cp -p`는 사용하지 않습니다. 파일 SHA256 및 기존 Stage15/16 버전이 일치하지 않으면 설치를 거부합니다.

**주의: `RTX3080-UNLOCK.EFI`는 Stage15의 고정 파일명 때문에 이렇게 부르지만, 이 단계에서 언락 기능은 없습니다.**

## 실행 및 로그

1. USB의 UEFI 항목을 일회성 부팅 메뉴에서 1회 선택합니다.
2. Stage15가 Windows Boot Manager를 선적재하고, Stage17 child를 호출합니다.
3. Stage17이 현재 GPU 및 GOP 토폴로지가 정확히 일치하는지 확인합니다.
4. 조건이 맞고 로그 기록이 가능하면 `DisconnectController(gpu,NULL,NULL)`를 실행합니다.
5. 분리 상태를 기록하고 `ConnectController(gpu,NULL,NULL,TRUE)`로 재연결을 시도합니다.
6. 다시 PCI 드라이버/자식 컨트롤러와 GPU GOP가 나타났는지 확인합니다.
7. 재연결 검증 성공 시 `EFI_SUCCESS`, 실패 또는 사전 검증 불일치 시 `EFI_ACCESS_DENIED`를 반환합니다.
8. 정상 반환한 경우 Stage15가 Windows로 인계합니다.

USB에서 확인할 파일:
- `RTX3080-STAGE17-RECONNECT.TXT` — Stage17 실제 상세 로그. (Stage16의 `RTX3080-STAGE16-PREFLIGHT.TXT`와 구별)
- 새로운 `RTX3080-STAGE15-YYYYMMDDhhmmss.TXT` — Windows 인계 및 child 반환 결과.

Stage17 성공 시:
```text
stage17_disconnect_status=0x0000000000000000
stage17_released_owner_bits=0x0000000000000000
stage17_connect_status=0x0000000000000000
stage17_restored_owner_bits=0x0000000000000003
stage17_restored_gop_count=0x0000000000000001
stage17_result=PASS_RELEASE_RECONNECT_NO_UNLOCK
stage17_child_return=EFI_SUCCESS_NO_UNLOCK
```
Stage15에는 `stage15_result=CHILD_RETURNED_SUCCESS_UNLOCK_UNVERIFIED`로 표시됩니다. 이 값은 **언락 성공이 아니라 EFI 모듈이 정상 반환했다**는 뜻입니다.

Stage17 실패 또는 차단 시 `stage17_result=SKIP_*`/`FAIL_*` 및 `stage17_child_return=EFI_ACCESS_DENIED_NO_UNLOCK`가 나옵니다. Stage15에는 `stage15_result=FAILED_CHILD_RETURNED`가 기록되고 Windows로 인계합니다. 등록해 놓은 Stage15 Windows 알림 스크립트는 이런 오류를 로그인 후 메모장에 표시할 수 있습니다.

## 복구 및 취소

부팅 전 취소하거나 부팅 후 Stage16 검증용 모듈로 되돌리려면:

```bash
bash fe/stage17/restore-stage16.sh /mnt/f
sync
```

이 명령은 정확한 백업 해시를 확인하고 1회 실행 마커를 제거합니다. 불확실한 파일을 덮어쓰지 않습니다.

검은 화면/컴퓨터 정지로 Windows에 진입할 수 없을 때에는 자동 복구를 보장할 수 없습니다. 수동 전원 종료 후 USB를 제거하고 원래 Windows 부팅 경로로 진입하는 것이 복구 경로입니다.

## 현재 검증 범위

- x64 UEFI PE 파일 clang/lld-link 빌드 성공
- Stage17 source direct PCI/MMIO 쓰기 없음 확인
- 설치/복원, 해시 검증, 중복 실행 차단 모의 테스트 통과
- **RTX 3080 FE 실기에서 UEFI 드라이버 분리/재연결은 아직 확인되지 않음**
- **SEC2/GSP 언락 미실행, CUDA 성능 변화 미검증**
