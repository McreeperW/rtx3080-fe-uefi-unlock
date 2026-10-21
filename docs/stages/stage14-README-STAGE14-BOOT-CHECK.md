# RTX 3080 FE Stage 14 — USB-to-Windows boot verification kit

대상: Ryzen 5 3600 / RTX 3080 Founders Edition / PCI `10DE:2206`, `0000:26:00.0`.

## 상태

- **Stage 14 언락 기능은 비활성화됨.** GPU 쓰기, DMA, FLR, 리셋, GOP 해제 및 언락을 수행하지 않는다.
- 기존 Stage 13b의 Windows Boot Manager 연결 기능 + 읽기 전용 소유권 안전 게이트를 실행한다.
- 실제 장비 Stage 14 UEFI 구동 및 Windows 진입 결과는 **아직 검증되지 않음**. Stage 13b 부팅 기록만 확인됨.
- 기존 소스 네이티브 정책 테스트 5개 및 정적 검사 통과, 제공 EFI SHA256 일치.

## 설치 전

Windows 디스크 관리에서 대상 USB 드라이브 문자를 확인한다. 아래 예시는 USB가 `F:`라고 **확인된 경우에만** 사용한다. Windows EFI 시스템 파티션(ESP)을 수정하면 안 된다.

WSL에서 기존 소스 작업 폴더로 압축을 푼다:

```bash
cd ~/code/rtx3080-fe-reference/rtx3080-unlock-public-main
unzip -o /path/to/rtx3080-fe-stage14-boot-validation-kit.zip -d .
bash fe/stage14/tests/run.sh
sha256sum fe/stage14/out/BOOTX64.EFI
```

정확한 SHA256: `e3f4de50a3991962158b645c977dbdf9a5a9db5afd964c3bf1b324efc9ffbbb4`

USB가 정말 `/mnt/f`일 때만:

```bash
ls -l /mnt/f/EFI/BOOT/BOOTX64.EFI
bash fe/stage14/install-usb.sh /mnt/f
cp RTX3080-POSTBOOT.ps1 /mnt/f/RTX3080-POSTBOOT.ps1
sync
```

기존 USB `EFI/BOOT/BOOTX64.EFI`는 `BOOTX64.STAGE14-BACKUP.EFI`에 백업된다. 백업이 이미 있으면 설치 스크립트는 덮어쓰기를 거부한다.

## 실제 1회 시험

1. Windows 종료 후 USB를 그대로 연결한다.
2. 메인보드의 일회성 부트 메뉴에서 UEFI USB를 선택한다. 부트 순서/NVRAM을 바꾸지 않는다.
3. Stage14 메시지가 잠깐 표시되고 Windows 로그인 화면으로 자동 전환되는지 확인한다. 추가 조작이나 GPU 리셋이 없어야 한다.
4. Windows 바탕화면이 정상 표시되면, 관리자 권한 없이 PowerShell에서 다음을 실행한다(USB가 F:인 경우):

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File F:\RTX3080-POSTBOOT.ps1
```

5. USB 루트의 **새** `RTX3080-BOOT-YYYYMMDDHHMMSS.TXT` 및 `RTX3080-STAGE14-WINDOWS-CHECK-YYYYMMDD-HHMMSS.TXT` 결과를 공유한다.

검증 스크립트는 Stage14 UEFI 바이너리 해시, 부트로그, 안전 게이트, Windows 로딩 상태, Windows의 RTX 3080 인식 및 NVIDIA 드라이버 응답을 검사한다. **실제 Windows 로그인과 NVIDIA 드라이버의 작동은 본체에서만 확인 가능**하다.

## 예상 로그

```
stage14_preflight_deny_mask=0x00000000000007bc
stage14_stage12_code_linked=0x0000000000000000
stage14_gpu_reset_attempted=0x0000000000000000
stage14_unlock_attempted=0x0000000000000000
stage14_decision=SKIP_UNLOCK_BOOT_WINDOWS
windows_bootmgr_discovered=1
windows_loadimage_status=0x800000000000000e
windows_loadimage_fallback=matching_boot_partition
windows_final_loadimage_status=0x0000000000000000
```

`0x7bc` 및 LoadImage 폴백은 이전 Stage13b 관측으로부터의 **예상치**이며 새로운 UEFI 실기 결과를 보장하지 않는다. `LoadImage` EFI_SUCCESS는 부트로더의 이미지 적재 성공일 뿐 Windows 완전 부팅 증거가 아니다.

## 복원

부팅에 실패하면 USB를 제거하고 기존 Windows Boot Manager로 부팅한다. USB를 다시 마운트하여 USB가 `/mnt/f`인 것을 확인한 뒤:

```bash
bash fe/stage14/restore-usb.sh /mnt/f
```

## 안전 게이트 이후

Stage 14 부팅이 통과해도 PCI Command `0x7`, NVIDIA GOP 소유권, 단일 GPU 환경에서 Stage12 실제 GPU 언락을 바로 켤 수는 없다. 드라이버/GOP 전환의 복구 경로, 리셋 실패 복구, Windows 부팅 호환성 등 별도의 실증이 선행되어야 한다. 이 패키지는 그런 조작을 전혀 수행하지 않는다.
