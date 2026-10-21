# RTX 3080 FE 실험용 직접 언락 경로 (Stage 12)

## 이 빌드가 하는 일

이것은 UEFI 실기 검증 전의 **고위험 실험용 트랜잭션 구현**입니다. 서명/ROM/배치 오프라인 검사가 통과했다고 GPU 펌웨어 실행 성공이 보장되지는 않습니다. NVIDIA VBIOS FLASH를 쓰는 코드는 포함하지 않지만, GPU 레지스터 쓰기, GPU DMA, 서명 펌웨어 실행, PCIe FLR을 수행할 수 있습니다. 실패하면 컴퓨터가 멈추거나 재부팅 후 GPU가 인식되지 않을 수 있습니다. 전원 차단/복구 작업이 필요할 수 있습니다. 위험을 감수하지 않는다면 사용하지 마세요.

- 기본 빌드(`fe/stage5/build-wsl.sh`)는 기존 읽기 전용 FE 차단 유지.
- 실험 빌드(`fe/stage12/build-experimental.sh`)는 `GU_FE_EXPERIMENTAL`로 FE 분기를 연결.
- 정확한 FE PCI ID, subsystem, GOP, BAR, cold MMIO, PROM SHA256/FWSEC 구조, 서명 퓨즈, 5개 자산 해시가 불일치하면 실행 중단.
- FE PCI MSE가 꺼져 있으면 `EFI_PCI_IO_PROTOCOL.Attributes`로 독점 소유한 상태에서 MSE만 활성화, BME 비활성 유지. 실패 시 시도 전 상태로 복구하거나 펌웨어 단계에서 중지.
- U를 누르기 전에는 GPU DMA/SEC2/GSP/FLR 및 SS0/SS1 쓰기 없음. U를 누르면 **실제** GPU 펌웨어 경로 진행.
- 원본 MSI 모드는 기본 빌드와 동일하게 유지.

## WSL에서 사용

1. `python3 fe/stage12/apply.py` (정확한 Stage 11 소스 SHA 확인 및 3개 파일 백업)
2. `bash fe/stage12/build-experimental.sh`
3. USB 경로를 확인하고, 위험을 수락한다면 `bash fe/stage12/install-usb.sh /mnt/e I_ACCEPT_GPU_RISK` (`/mnt/e`는 실제 USB에 맞게 바꾸기)
4. USB를 통해 콜드 부팅하고, 전면 화면에 위험 경고가 표시되면 명시적으로 `U` 입력. Esc 또는 시간 초과는 취소.
5. 실패/정지 시 물리 전원 버튼으로 완전히 전원을 끄고, 기존 EFI를 복구하세요. GPU가 돌아오면 안전한 부팅 상태에서 `/RTX3080-UNLOCK/RUNxx/REPORT.TXT` 확인.
6. 성공 후 Windows에서 동일한 FP16/FP32 벤치마크를 실행해 실제 성능 변화 측정.

## 준비 상황

현재 파일은 코드 빌드 및 정적 검증만 완료됐고, 실물 RTX 3080 FE에서의 실행은 **검증되지 않았습니다**. 65kB대 읽기 전용 EFI와 구별하여 실험 파일명에 `FE-EXPERIMENTAL`을 붙였습니다. 이 기능은 VBIOS를 영구적으로 수정하는 도구가 아닙니다.

## 중요: 생성물과 복원

`fe/stage12/build/BOOTX64.FE-EXPERIMENTAL.EFI`는 **실험용**이며 네 RTX 3080 FE 하드웨어에서 실행 성공이 검증된 적 없습니다. `unlock/build/BOOTX64.EFI`는 마지막으로 사용한 빌드 스크립트에 따라 덮어써질 수 있으므로, 실험용 산출물은 이름에 `FE-EXPERIMENTAL`이 붙은 파일만 사용하세요. 평소용 읽기 전용 EFI로 돌아가려면 `python3 fe/stage12/restore.py && bash fe/stage5/build-wsl.sh`를 사용합니다. 실제 그래픽카드에 대한 안전성이나 성공을 보장하는 코드는 아닙니다.

실험용 실행 중 GPU가 응답하지 않으면 소프트웨어에서 강제 재시도하거나 반복 부팅하지 말고 전원을 완전히 차단하고 ROM 백업·다른 디스플레이 경로를 유지하세요. 로그를 확보하기 전에는 추가 실험을 진행하지 않는 것이 좋습니다.

### PCI Command 변경 시점과 실패 시 동작

기본적으로 Windows 부팅 전 FE의 Memory Decode는 OFF이므로, 실험용 EFI는 **U를 누르기 전에도** PCI MSE를 일시적으로 켭니다. 따라서 U 입력 전의 과정도 완전한 무쓰기 진단은 아닙니다. 사용자가 U를 누르지 않거나 GPU 쓰기 이전에 오류가 나면 MSE 복원을 검증합니다. GPU 레지스터 쓰기가 시작된 뒤 오류가 발생하면 Windows로 진행하지 않고 전원 차단을 요구하는 정지 루프에 들어갑니다. 이 동작이 GPU의 원상복구를 증명하지는 않습니다.
