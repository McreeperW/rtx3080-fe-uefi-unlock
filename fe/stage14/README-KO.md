# RTX 3080 FE Stage 14 — ownership gate + Stage 13b Windows boot bridge

**버전 성격: Stage 14 안전 게이트 진입 빌드 (언락 수행 빌드 아님).**

기반: 사용자가 제공한 Stage 13b (`fe/stage13b/src/bootbridge.c`), 앞서 제공된 Stage 12 실험 소스, 2026-10-21 실제 RTX 3080 FE UEFI 로그.

## 실제 시스템에서 확인한 상태

* Ryzen 5 3600에는 iGPU가 없고, RTX 3080 FE가 UEFI GOP/모니터 출력을 담당합니다.
* GPU PCI ID `10DE:2206`, 관측 BDF `0000:26:00.0`, PCI Command `0x0007`.
* GPU PCI 오픈 정보에 `0x10` (`BY_DRIVER`), `0x08` (`BY_CHILD_CONTROLLER`)가 있으며 GOP에도 `0x10` (`BY_DRIVER`)가 있습니다.
* 기존 Stage 13b 로그에서 `Boot0000` 탐지 후 첫 `LoadImage`는 `EFI_NOT_FOUND`, 정확히 일치하는 ESP를 찾아 재시도하면 `EFI_SUCCESS`.

## 이 빌드에서 실제로 실행하는 경로

1. UEFI USB 부팅 → 로그 파일 열기 → RTX 3080 FE PCI 정보, GOP, UEFI 소유권을 읽습니다.
2. 읽기 전용 Stage 14 정책 `s14_gate()`로 소유권 충돌과 복구 불확실성을 평가합니다.
3. 현재 장비에서는 PCI command `0x7`, 활성 GOP, 드라이버 소유권, 복구 미입증 조건 때문에 반드시 언락이 차단됩니다.
4. 변수를 바꾸거나 NVIDIA GPU를 리셋하지 않고 Stage 13b Windows Boot Manager 로딩을 실행합니다.
5. 로그는 USB 루트 `RTX3080-BOOT-YYYYMMDDHHMMSS.TXT`에 저장됩니다.

**Stage 12의 GPU 레지스터 쓰기, DMA, SEC2/GSP, 펌웨어 실행, FLR 트랜잭션은 이 바이너리에 포함하지 않았습니다.**
소스에 있는 `stage14_stage12_code_linked=0`, `stage14_unlock_attempted=0`, `stage14_gpu_reset_attempted=0`가 이 사실을 명시합니다.
즉 목표인 **'언락 후 Windows 무인 부팅'은 아직 달성되지 않았습니다.**

## 사전 검증 결과

`tests/run.sh`로 호스트 네이티브 안전 정책 시나리오 5건과 소스 쓰기 명령 차단 검사를 통과했습니다. Clang + lld-link PE32+ / EFI application 바이너리 컴파일이 성공했습니다. 실제 펌웨어에서 **Stage 14 자체를 부팅한 테스트는 아직 진행하지 않았습니다.**

## 빌드 방법 (WSL/Linux)

정상적으로 이전 프로젝트가 체크아웃된 `~/code/rtx3080-fe-reference/rtx3080-unlock-public-main`에서:

```bash
unzip -o rtx3080-fe-stage14-guarded-boot.zip -d .
bash fe/stage14/tests/run.sh
EDK2_HEADERS="$PWD/deps/edk2/MdePkg/Include" bash fe/stage14/build.sh
```

혹은 제공된 `fe/stage14/out/BOOTX64.EFI`를 그대로 사용할 수 있습니다.

## USB 설치 및 안전 복원

**먼저 Windows에서 USB가 정말 `F:`인지 디스크 관리에서 확인하십시오.** `F:` USB는 FAT32, 아래 경로의 원본 EFI가 남아 있어야 합니다.

```bash
# WSL에서 드라이브 마운트가 이미 되어 있고 실제 USB가 /mnt/f인지 확인
ls -l /mnt/f/EFI/BOOT/BOOTX64.EFI
bash fe/stage14/install-usb.sh /mnt/f
```

`EFI/BOOT/BOOTX64.STAGE14-BACKUP.EFI`에 기존 EFI를 최초 1회 백업하고 Stage 14 EFI를 설치합니다. 이전 Stage 13의 보조 백업을 덮어쓰지 않습니다. 백업 파일이 이미 있으면 안전상 덮어쓰기를 거부합니다.

USB를 **일회성 Boot Menu**로 선택하여 1회 부팅합니다. 정상이라면 언락 건너뛰기 로그를 남기고 Windows로 들어갑니다. USB 자체의 부팅 순서/NVRAM은 변경하지 않습니다.

USB 부팅에 실패하거나 원래 Stage 13/13b로 복구할 경우:

```bash
bash fe/stage14/restore-usb.sh /mnt/f
```

### 예상되는 Stage 14 로그 필드

- `stage14_preflight_deny_mask=0x...`: 아래 비트들의 OR. **0이면 이론상 가드 통과일 뿐, 실제 언락 성공이나 실행 가능을 보장하지 않습니다.**
- `stage14_driver_bound=0x...`: NVIDIA GPU의 PCI/GOP UEFI 드라이버 점유 감지.
- `stage14_gop_routes=0x...`: 활성 RTX 3080 GOP 경로 수.
- `stage14_stage12_code_linked=0x0000000000000000`.
- `stage14_gpu_reset_attempted=0x0000000000000000`.
- `stage14_unlock_attempted=0x0000000000000000`.
- `stage14_decision=SKIP_UNLOCK_BOOT_WINDOWS`.
- 기존과 동일하게 `windows_final_loadimage_status=0x0000000000000000`이면 Windows Boot Manager 이미지 로드 성공을 뜻합니다. 실제 Windows 부팅 완료는 별도 관찰이 필요합니다.

#### 거부 마스크

| 비트 | 16진 | 조건 |
|---|---:|---|
| 0 | 0x001 | 대상 FE GPU 수가 1이 아님 |
| 1 | 0x002 | PCI 신원/위치/관측 실패 |
| 2 | 0x004 | PCI I/O decode 활성 |
| 3 | 0x008 | PCI memory decode 활성 |
| 4 | 0x010 | PCI bus mastering 활성 |
| 5 | 0x020 | RTX 3080에 GOP 경로 존재 |
| 6 | 0x040 | 소유권 정보 조회 실패 |
| 7 | 0x080 | 드라이버가 PCI/GOP 점유 |
| 8 | 0x100 | child controller로 연결됨 |
| 9 | 0x200 | 리셋 및 그래픽 복원 실기 검증 없음 |
| 10 | 0x400 | Stage 12 언락 트랜잭션 통합/실기 검증 없음 |

기존 2026-10-21 로그와 신원/소유권 결과가 동일하면 `0x7bc`가 예상됩니다. 이 수치는 예상치일 뿐 새 USB 실기 로그로 확인해야 합니다.

## 남은 연구 단계

현재 GPU는 화면 출력 중이며 PCI command=7입니다. 다른 디스플레이 경로가 없으므로 GOP를 끊거나, GPU FLR/리셋을 자동으로 실행하면 그래픽 출력을 잃을 수 있습니다. UEFI Driver Model 사양상 EXCLUSIVE 획득이 드라이버의 `DisconnectController` 실행을 유발할 수 있으므로, 이 빌드는 EXCLUSIVE 획득 자체를 수행하지 않습니다.

Stage 12를 붙이려면 (a) 디스플레이/GOP를 포함한 소유권 전환 및 복귀를 실제 장비에서 입증하고 (b) 리셋 후 GPU와 Windows 부팅 가능성을 검증하며 (c) Stage 12 DMA/펌웨어 전송·트랜잭션 실패의 복구 동작을 감사해야 합니다. 그 전에는 자동 언락을 켤 수 없습니다.
