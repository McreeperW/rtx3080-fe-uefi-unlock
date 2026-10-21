# RTX 3080 FE + Ryzen 5 3600: Stage 13 읽기 전용 자동 부트브리지

## 목적
- **RTX 3080 단독 출력 환경**에서 PCI, GOP, 장치 경로만 조회한다.
- 기존 펌웨어 `BootOrder`의 Windows Boot Manager 경로(`\\EFI\\Microsoft\\Boot\\bootmgfw.efi`)를 판별한다.
- 키 입력 없이 `LoadImage`/`StartImage`를 호출해 **Windows 부팅으로 자동 전달**한다.
- Boot####/BootOrder/BootNext를 바꾸지 않는다. 내장 Windows EFI 파일도 건드리지 않는다.

**절대 언락 EFI가 아니다. FP16 속도는 변하지 않는다.** GPU DMA, MMIO 쓰기, MSE/BME 변경, SEC2/GSP, FLR, 드라이버 분리 없음. Stage 12 실험용 EFI는 USB `RTX3080-UNLOCK/BACKUP/`으로 백업된다.

## 적용 대상
이전 Stage 12를 FAT32 USB (`/mnt/f` 등)에 넣은 상태에서:

```bash
cd ~/code/rtx3080-fe-reference/rtx3080-unlock-public-main
unzip -o /mnt/c/Users/pc/Downloads/rtx3080-fe-stage13-auto-boot.zip -d .
bash fe/stage13/install-usb.sh /mnt/f
```

이제 USB로 수동 부팅하면 **키 입력 없이** 로그 수집 후 Windows Boot Manager를 시작한다. Windows Boot Manager는 펌웨어에 이미 등록된 `BootOrder` 항목을 사용한다. UEFI 시스템별 실행 호환성은 실물 검증 전이며, 자동 부팅이 실패하면 Windows를 펌웨어 부팅 메뉴에서 직접 선택하면 된다. USB를 제거하면 원래대로 부팅 가능하다.

USB 루트에 `RTX3080-BOOT-YYYYMMDDhhmmss.TXT` 보고서 생성. `windows_bootmgr_discovered=1`, `windows_loadimage_status=0` 및 실제 Windows 부팅이 확인되면 **부팅 체인 부분만** 성공이다. `windows_startimage_return_status`가 출력되면 Windows Boot Manager가 EFI로 되돌아온 것이므로 실패 원인을 확인한다.

## 원래 USB EFI 복원

```bash
bash fe/stage13/restore-usb.sh /mnt/f
```

복원은 Stage 12 **실험용 GPU 쓰기 EFI**를 다시 배치하므로 실수로 자동 실행하지 말 것.

## 소스와 빌드
- `src/bootbridge.c`: 독립 UEFI 코드. Intel/AMD GOP를 가정하지 않는다.
- `build.sh`는 EDK2 `MdePkg` 헤더가 필요하다.

```bash
EDK2_HEADERS="$PWD/deps/edk2/MdePkg/Include" bash fe/stage13/build.sh
```

## 다음 단계 (이번 파일에는 없음)
1. Stage 13 결과로 RTX 3080이 **활성 콘솔 GOP를 직접 제공하는지** 확인.
2. 구동 중인 NVIDIA GOP/PCI 드라이버를 분리하면 Windows 부팅 경로가 보존되는지 별도 **수동 실험**으로 확인.
3. 그 과정에서 안전한 리셋·오류 복구가 실증될 때만 무인 언락 플래그를 도입.
4. 실패 시 일반 Windows로 우회하는 별도 부팅 항목을 유지. 영구 VBIOS 개조는 하지 않음.

## 제한 사항
- 미서명 EFI이므로 Secure Boot 활성화 환경에서는 실행이 차단될 수 있다.
- Windows Boot Manager는 EFI `BootOrder` 항목 중 표준 Microsoft 경로만 인정하며, 제조사 특수 구성은 자동 인식하지 못할 수 있다.
- PC 전원 차단 후 기존 Tensor 성능 언락은 초기화되는 것으로 사용자가 관측했다.
