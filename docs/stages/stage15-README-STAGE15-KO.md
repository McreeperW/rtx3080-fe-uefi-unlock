# RTX 3080 FE Stage 15: Headless failure-to-Windows boot bridge

## 실제 검증 범위

- 출발점: **실기 통과 Stage 14 Windows handoff** (`e3f4de50a399...`), 같은 부트옵션 디바이스 패스 폴백 사용.
- 과거 FE Stage 12에서 Ryzen 2400G + AMD GOP로 실험한 `REPORT(10).TXT`: RTX 3080 FE의 SS0/SS1 설정, FLR 및 펌웨어 트랜잭션 `result=0`이 확인됨.
- **하지만 Ryzen 3600 + 유일한 RTX 3080 GOP를 점유한 현재 시스템에서는 Stage 12 성공이 재현되었다는 근거가 없음.** 그때 사용한 Stage 12 바이너리는 AMD GOP와 수동 `U`, post-write 실패시 무한 정지 로직을 포함하므로 **Stage15에 그대로 끼워서 무인 실제 언락용으로 사용하지 말 것**.
- 이 패키지에 포함된 `BOOTX64.EFI`는 **GPU를 변경하는 언락 엔진이 아닌 실행기**다. 외부 `RTX3080-UNLOCK.EFI`가 따로 존재하고, 1회 실행 마커를 만들었을 때만 해당 EFI를 실행한다. 패키지 자체만으로는 Tensor unlock을 하지 않는다.
- 컴파일 및 정책 테스트를 수행했지만, **Stage15 신형 EFI는 아직 현재 PC에서 실기 테스트하지 않았다.**

## 실행 순서

1. USB EFI를 Stage 15로 교체한다(아래 명령).
2. `RTX3080-STAGE15-ARM.ONCE`가 없는 상태에서 먼저 USB 부팅한다. 기존 Stage 14와 같이 Windows로 자동 진입하면 Stage 15 설치와 선로딩이 검증된 것이다.
3. Windows 로그인 후 아래 Windows 알림 도구를 설치한다. 이 도구는 **새로운 Stage 15 실패 또는 미반환 로그가 있을 때만** 메모장을 연다. 성공/스킵은 조용히 처리한다.
4. RTX 3080 FE + Ryzen 3600 **단독 GPU 환경에서 성공·실패 후 안전하게 반환 가능하도록 포팅된 언락 EFI가 확보됐을 때만** `EFI/BOOT/RTX3080-UNLOCK.EFI`로 설치한다. 현재 제공된 Stage 12 파일은 이 조건을 충족하지 않는다.
5. 그때 `bash fe/stage15/arm-once.sh /mnt/f`로 다음 USB 부팅 **딱 1회** 언락 모듈을 실행할 수 있다.

## 설치 및 실행

이 ZIP을 프로젝트 루트에 압축 해제한 뒤:

```bash
bash fe/stage15/tests/run.sh
bash fe/stage15/install-usb.sh /mnt/f
sync
```

EFI를 직접 다시 빌드하려면: `bash fe/stage15/build.sh` (정확한 EDK2 헤더와 clang/lld-link 설치 필요).

Windows 로그인 후 ZIP의 루트 디렉터리에서 PowerShell로:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Install-Stage15-Windows-Notifier.ps1
```

Windows 시작 시 USB 루트의 `RTX3080-STAGE15-*.TXT`를 찾아 실패 로그만 자동으로 표시한다. 자동 실행을 중지하려면 `Remove-Stage15-Windows-Notifier.ps1`을 실행한다.

복원:

```bash
bash fe/stage15/restore-usb.sh /mnt/f
```

## 내부 동작

1. RTX 3080 PCI·GOP 관측 (GPU 쓰기 없음).
2. BootOrder의 Windows Boot Manager 이미지 파일을 먼저 읽어 메모리에 `LoadImage` 성공시키고 핸들을 보존.
3. 정확한 확인 문자열 `I_ACCEPT_GPU_HANG_RISK`가 든 1회 실행 마커 발견 → **마커 파일을 먼저 삭제**. 삭제가 실패하면 언락 실행 금지.
4. USB의 `EFI/BOOT/RTX3080-UNLOCK.EFI`를 `LoadImage`하고 `StartImage` 실행. 이 EFI는 별도 개발·검증 대상이며 이 패키지에 포함되지 않는다.
5. 외부 EFI가 반환하면 `stage15_result=FAILED_CHILD_RETURNED` 또는 `CHILD_RETURNED_SUCCESS_UNLOCK_UNVERIFIED` 기록, Windows를 `StartImage`.
6. 파일을 찾지 못하거나 확인 마커가 없으면 별도 GPU 작업 없이 Windows를 실행한다.

**무한 정지/펌웨어 실행 도중 GPU 행/PCIe FLR hang/보드 펌웨어 freeze에는 이 구조의 후속 Windows 체인로딩이 보장되지 않는다.** EFI가 반환하지 않으면 같은 UEFI 실행 흐름에서 Windows를 시작시킬 수 없다. 다음 물리적 콜드 부팅에 재시도되지 않게 하는 것과 단일 부팅 중 복구하는 것은 별개다.

### 로그 상태

- `stage15_result=SKIPPED_NOT_ARMED`: 마커 없음. GPU 변경 없음.
- `stage15_result=FAILED_MARKER_NOT_CONSUMED`: 마커 삭제 실패, 위험 동작 건너뜀.
- `stage15_result=FAILED_CHILD_LOAD`: 외부 EFI 파일 없음/로드 실패.
- `stage15_result=FAILED_CHILD_RETURNED`: EFI가 에러 반환. Windows에 연결 시도.
- `stage15_result=CHILD_RETURNED_SUCCESS_UNLOCK_UNVERIFIED`: EFI 정상 반환, 실제 언락 여부는 Windows FP16/BF16 성능 검증 전까지 미확정.
- `stage15_result=ATTEMPT_STARTED_NOT_YET_RETURNED`만 마지막에 남으면 모듈이 정상 반환하지 못했거나 기록이 중단됐을 수 있음. 다음 Windows 로그온에서 오류로 표시됨.
- `stage15_windows_handoff=1`: Windows StartImage 직전까지 도달. Windows 로그인 확인은 별도.

### 중요한 안전 한계

- 일회성 마커 외에 NVRAM BootOrder/VBIOS는 변경하지 않는다.
- 본 브리지 바이너리는 GPU MMIO/PCI 쓰기, DMA/FLR 및 UEFI `DisconnectController`를 호출하지 않는다. **외부 EFI 동작은 이 문장에 포함되지 않는다.**
- 실제 언락 모듈은 이전 성공 로그(2400G+AMD GOP)와 현재 NVIDIA GOP 소유권 조건의 차이를 해결해야 한다. `DisplayCheck` 조건만 제거하거나 `Halt()`를 Windows 부팅으로 대체하는 것은 안전한 포팅이 아니다.
- 반드시 USB 원본 EFI를 복구할 수 있어야 하며, 단독 GPU에서 강제 소유권 해제/FLR 후 Windows가 실행될 것이라고 가정하지 말 것.
