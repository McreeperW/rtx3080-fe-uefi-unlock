# USB 자동 언락 (실험적)

이미 **Stage 21 언락 + Windows 부팅 + FP16/BF16 성능**을 확인한 **동일 PC·동일 USB**에만 적용합니다. **GPU 정지 및 반복 부팅 실패 위험**이 있습니다. 다른 시스템에서 사용하지 마세요.

## 동작

- 기존: `RTX3080-STAGE15-ARM.ONCE` 한 번 실행 후 삭제.
- 자동: `RTX3080-STAGE15-AUTO.ON`을 유지해 **USB 부팅마다** 언락 시도.
- 실행 전 `RTX3080-STAGE15-AUTO.PENDING` 기록.
- **Stage 20 EFI가 성공 반환한 경우에만** `AUTO.PENDING` 삭제.
- 정지·오류 시 `AUTO.PENDING` 유지 → 다음 USB 부팅에서 언락 생략, Windows로 인계 시도.
- 이 성공 판정은 **Windows 부팅이나 NVIDIA 드라이버/성능을 보증하지 않음**.
- USB가 없으면 원래 Windows 부팅 경로를 사용합니다. SSD EFI 파티션은 변경하지 않습니다.

## 적용 (WSL, 기존 성공 USB)

USB가 `F:`이고 `/mnt/f`에 마운트된 경우:

```bash
cd ~/code/rtx3080-fe-uefi-unlock
bash fe/stage15/build.sh
bash scripts/install-auto-usb.sh /mnt/f
```

설치기는 **검증된 Stage 15·20·21과 ASSETS 5개**의 해시를 확인하고, `BOOTX64.EFI`만 교체합니다. 기존 부트로더는 `RTX3080-STAGE15-BOOTX64-ONESHOT.BAK`로 백업합니다. **Stage 20·21 엔진과 ASSETS는 수정하지 않습니다**.

설치 후 확인:

```bash
python3 scripts/check-usb.py --usb /mnt/f --require-auto
```

이제 PC의 UEFI 부팅 메뉴에서 해당 USB를 선택하면, 마커를 매번 수동으로 만들지 않아도 실행을 시도합니다. USB가 기본 부팅 장치로 설정돼 있어야 전원 켤 때마다 자동 진입합니다. **부팅 순서 변경은 이 프로젝트가 하지 않습니다.**

## USB 구조

```text
USB/ (FAT32)
├── EFI/BOOT/
│   ├── BOOTX64.EFI                   # 새 Stage 15 자동 모드 지원
│   ├── RTX3080-UNLOCK.EFI            # 성공한 Stage 20 그대로
│   └── RTX3080-STAGE19-ENGINE.EFI    # 성공한 Stage 21 그대로
├── RTX3080-UNLOCK/ASSETS/            # 기존 5개 그대로
└── RTX3080-STAGE15-AUTO.ON           # 자동 모드 허용
```

### 중지

```bash
bash scripts/disable-auto-usb.sh /mnt/f
```

`AUTO.ON`만 삭제합니다. `AUTO.PENDING`이 남았다면 **지난 부팅 실패 신호**이므로 함부로 지우거나 재시도하지 마세요. USB를 제거해 일반 Windows로 복구할 수 있습니다.

### 로그

`RTX3080-STAGE15-*.TXT`에서 `stage15_mode=AUTO_PERSISTENT_LOCKED`, `stage15_auto=REARMED_FOR_NEXT_USB_BOOT`를 확인합니다. 다음 부팅부터 자동으로 언락되더라도, **실제 성능은 필요한 경우 동일한 FP16/BF16 벤치마크로 확인**해야 합니다.

## 제한

- 자동 Stage 15는 **컴파일과 정적 검증만 완료**, 실기 부팅 미검증.
- GPU 펌웨어 트랜잭션·PCIe FLR 과정에서 검은 화면, 시스템 정지 및 데이터 손실 가능.
- Stage 20에서 성공을 반환한 뒤 Windows 인계가 실패하면 잠금이 이미 해제돼 다음 USB 부팅에서 반복될 수 있음. **USB를 빼서 복구**해야 합니다.
- 부팅마다 적용된다는 뜻이지, **USB 없이도 영구 언락된다는 의미가 아닙니다**.
- 초기 1회 실행 모드로 복귀하려면 `AUTO.ON` 삭제 후 기존 EFI 백업 복원(USB에서만 수행); 일회성 마커는 정확히 `I_ACCEPT_GPU_HANG_RISK`라는 내용이 필요합니다.
