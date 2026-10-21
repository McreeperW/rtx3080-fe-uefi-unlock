# RTX 3080 FE Stage 16 — Headless child return-to-Windows integration test

**이것은 언락 EFI가 아니다.** 패키지 내 `fe/stage16/out/RTX3080-UNLOCK.EFI` 파일명은 이전 Stage 15 실행기가 요구하는 파일명과 맞추기 위한 것이며 실제로는 **읽기 전용 사전검사 모듈**이다.

## 목적

실기 검증된 Stage 15(USB EFI -> Windows 이미지 선적재 -> Windows 실행)에 외부 EFI를 **처음으로 연결**하여, 외부 EFI가 오류를 반환하면 Stage 15가 결과를 기록하고 Windows로 부팅할 수 있는지 검증한다. 현재 NVIDIA GOP가 점유 중인 유일한 RTX 3080 FE의 UEFI 소유권 구조(드라이버/자식 컨트롤러)도 상세 기록한다.

- 이 모듈은 GPU MMIO 쓰기, PCI 구성 쓰기, FLR, 펌웨어 실행, GPU 드라이버 분리/재연결을 하지 않는다.
- 모든 경로에서 의도적으로 `EFI_ACCESS_DENIED`를 반환한다. 따라서 Stage 15에는 `stage15_result=FAILED_CHILD_RETURNED` 로그가 남는 것이 **정상**이다.
- 실제 언락을 시도하지 않으므로 이 테스트로는 언락 트랜잭션 성공·실패 복구 가능성을 검증하지 못한다.
- Stage 12에서 2400G+AMD GOP 구성으로 트랜잭션 성공 결과가 존재하지만, 현재 Ryzen 3600+NVIDIA GOP 단독 출력에 그대로 사용할 수 없다.

## 실행

Stage 15가 이미 설치된 USB(F:)를 WSL에서 `/mnt/f`로 마운트한 뒤 프로젝트 루트에서:

```bash
unzip -o /mnt/c/Users/pc/Downloads/rtx3080-fe-stage16-child-return-test.zip -d .
bash fe/stage16/tests/run.sh
bash fe/stage16/install-once.sh /mnt/f
sync
```

설치 스크립트는 Stage 15 BOOTX64.EFI 해시가 일치해야만 동작하고, 기존 child나 1회실행 마커가 이미 있으면 덮어쓰지 않는다. `RTX3080-STAGE15-ARM.ONCE` 마커를 설치 과정에서 만든다. **따라서 USB를 선택한 다음 1회만 자식 모듈이 실행된다.** 이후 Stage 15 부팅은 다시 마커 없음으로 진행된다.

### 부팅 후 확인할 로그

USB 루트:
- `RTX3080-STAGE16-PREFLIGHT.TXT` — Stage 16 상세 기록. `stage16_result=SKIP_ACTIVE_CHILD_CONTROLLER` 등이 정상적인 차단 결과다.
- `RTX3080-STAGE15-YYYYMMDDhhmmss.TXT` — Stage 15 외부 EFI 로드·반환 및 Windows 인계. 아래와 유사한 내용이 있어야 한다.

```text
stage15_windows_preloaded=1
stage15_marker_status=0x0000000000000000
stage15_child_load_status=0x0000000000000000
stage15_child_returned=1
stage15_child_startimage_return=0x800000000000000f
stage15_result=FAILED_CHILD_RETURNED
stage15_windows_handoff=1
```

주의: `EFI_ACCESS_DENIED`의 실제 UEFI 값은 `0x800000000000000f` (EFIERR(15))이므로, 출력이 다르면 원인을 확인할 것.

Windows 진입 후 예전 `RTX3080-STAGE15-NOTIFY.ps1`를 등록했다면 새로운 실패 로그를 메모장에 보여준다. 이번 실패 표시는 **의도한 성공적인 테스트 결과**다.

다시 원래 Stage 15(외부 모듈 없음)로 돌아가려면:

```bash
bash fe/stage16/remove-child.sh /mnt/f
```

## 다음 실제 언락 모듈에 필요한 요건

1. NVIDIA GOP가 해당 FE에 연결된 PCI Driver Binding 및 child controller를 갖는 현재 단독 GPU 소유권 토폴로지 처리.
2. GPU 리셋/FLR 후 EFI 그래픽 드라이버를 다시 붙일 수 있는지 실기 검증.
3. 기존 Stage 12의 ROM/서명/GSP 검사와 트랜잭션 결과/clean up 로직 유지.
4. 트랜잭션에서 정상 반환이 확인된 경우에만 Stage15가 Windows로 넘기도록 하고, 무응답·GPU 손상 가능성이 있는 실패에 대해서는 Windows 자동 복귀를 **보장할 수 없음을 명시**.

**이 패키지는 실제 언락 기능이 없으며 단독 GPU의 드라이버 분리·재연결 안전성이 검증되지 않았다.**
