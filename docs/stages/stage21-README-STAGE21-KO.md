# RTX 3080 FE Stage 21 — 콜드 상태 변형 제한적 허용 (실험)

**실험 이미지입니다. GPU DMA, FWSEC/SEC2, GSP 레지스터 변경, PCIe FLR을 실행할 수 있으며, 실제 하드웨어 재시험 전까지 작동·복구를 보장하지 않습니다.** 지난 Stage 19 성공 이후 Stage 20에서 보고된 `SS0=0x10140014`에 대해, 다른 콜드 상태 조건이 *모두* 일치하는 경우에 한해서만 엔진이 다음 검증 단계로 진행하도록 수정했습니다.

## 수정 범위

- Stage 20의 `BOOTX64.EFI`, `RTX3080-UNLOCK.EFI` 브리지는 바꾸지 않습니다.
- Stage 20 브리지가 참조하는 `EFI/BOOT/RTX3080-STAGE19-ENGINE.EFI`만 Stage 21 엔진으로 교체합니다.
- `fe_cold.h`의 기존 허용 SS0 값 세 가지(0x10240014, 0x10241014, 0x10341014)에 실측 `0x10140014`를 추가했습니다. GPU 칩, GFW, WPR2, PLM, SS1, Fuse, 기타 확인 조건은 그대로입니다.
- `REPORT.TXT`에 모든 ColdCheck 판단 입력값을 `STAGE21 cold detail`로 기록합니다. 다른 조건이 맞지 않으면 **EFI_NOT_READY를 반환하고 언락을 시도하지 않습니다**.
- 기존 Stage 12의 위험 상태 무한 정지(Halt) 로직을 제거하거나 우회하지 않습니다.

## 설치 (기존 Stage20 USB만 지원)

```bash
cd ~/code/rtx3080-fe-reference/rtx3080-unlock-public-main
unzip -o '/mnt/c/Users/pc/Downloads/rtx3080-fe-stage21-guarded-coldstate.zip' -d .
bash fe/stage21/tests/run.sh
bash fe/stage21/install-once.sh /mnt/f
sync
```

설치기는 기존 Stage 15 BOOTX64, Stage 20 브리지, Stage 19 엔진, Stage 18 원본 백업 및 NVIDIA 펌웨어 5종을 **정확한 해시**로 검증한 후 Stage 19 엔진을 백업하고 한 번만 실행되도록 설정합니다.

USB에서 예상 결과 확인: `RTX3080-UNLOCK/RUNxx/REPORT.TXT`, `RTX3080-STAGE20-UNLOCK.TXT`, `RTX3080-STAGE15-*.TXT`. 최신 `REPORT.TXT`의 `STAGE21 cold detail`와 `FINAL` 값이 핵심입니다. `stage20_result=PASS...`가 나와도 Windows에서 `nvidia-smi` 및 실제 FP16/BF16 계산 성능 측정 전에는 최종 성공 판정을 내릴 수 없습니다.

## 복원

```bash
bash fe/stage21/restore-stage20.sh /mnt/f
sync
```

복원 시 최초 Stage19 엔진을 되돌리고 실행 마커를 제거합니다. Stage 20 브리지는 그대로 유지되므로 언락 시도 없이 Windows로 넘어가는 구조입니다.

## 한계

`0x10140014`는 관측된 값일 뿐, 정상 초기 상태임이 독립 검증된 값이 아닙니다. 다른 입력이 일치하는 경우라도 GPU 펌웨어 트랜잭션이나 Windows 드라이버가 정상일 것이라고 보장할 수 없습니다. 단독 GPU이므로 화면이 꺼지거나 멈출 수 있으며 실패 시 강제 종료 후 USB를 제거하고 부팅해야 할 수 있습니다. 무한 정지 중에는 Windows 실패 로그가 남지 않을 수 있습니다.

Built file SHA-256: `0d578a0612fb8a59b7320a0e038d2f19b03f9573b2a995f10ed44e6e10b32373`.
