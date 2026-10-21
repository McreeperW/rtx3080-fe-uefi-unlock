# Stage 21용 NVIDIA 펌웨어 ASSETS 준비 및 검증

> **상태:** 생성 명령은 이전 기록에서 확인됨. 새 생성물은 반드시 크기·SHA-256 검증.

## 1. 왜 Git 저장소에 없는가?

이 프로젝트는 실제 NVIDIA 드라이버/펌웨어에서 준비한 바이너리와, 관련 파생 생성물을 사용합니다. 라이선스·저작권 문제가 있을 수 있으므로 Git 저장소와 릴리스에 자산 바이너리를 동봉하지 않습니다. **이 파일은 본 저장소의 `build-all.sh`로 생성되지 않으며, 별도 `gpu-unlock`의 `make assets`로 준비합니다.**

원래 Stage 12 실험에서 확인한 동일한 자산을 Stage 19/20/21에서 재사용했습니다. 해당 바이너리를 별도로 보유하지 않은 사용자가 저장소만 받아 설치 스크립트를 실행하면 실패하도록 설계됐습니다.

## 2. 필요한 정확한 파일 5개

| 파일명 | 크기 (바이트) | SHA-256 |
|---|---:|---|
| `booter_load_ga102.bin` | 61,256 | `056cbc064a888f80717726c54d7f64fcd68044d123870c95a74f37f4975dcaa4` |
| `v67-ga102-610.43.03.bin` | 64,000 | `2a0aa1f86d0b6afff5214d641d87d46dc98ef06ed908ff05d0ddbad5ffb7e197` |
| `gsp_ga10x.bin` | 84,277,400 | `572373620a37418f24dc16b5031c39338778c3257e48e8408de9a57291b24f3a` |
| `gsp_boot_ga102.bin` | 24,576 | `3df651acb174fd5f5561a1996965b192f50c5d763168446ed0d4ead19d895768` |
| `gsp_boot_desc_ga102.bin` | 84 | `cc5a3fc1f4c47f2fe107b1b68f0176d8a448e6dec84b575e5560ccef5262bf15` |

이 목록의 단일 원본은 [`unlock/assets.json`](../unlock/assets.json)입니다. Stage 21 설치 스크립트 [`fe/stage21/install-once.sh`](../fe/stage21/install-once.sh)도 파일명·크기·해시를 확인합니다.

## 3. ASSETS 생성 — 이전 실험 기록

2026-10-20 작업에서 [`Zagitta/gpu-unlock`](https://github.com/Zagitta/gpu-unlock)의 `Makefile`을 사용했습니다. 원본 입력 버전은 **610.43.03**입니다.

```bash
# WSL/Linux; 첫 실행 시
git clone https://github.com/Zagitta/gpu-unlock.git ~/gpu-unlock
make -C ~/gpu-unlock assets

# RTX 3080 FE 언락 저장소 루트에서 실행
python3 scripts/verify-assets.py ~/.cache/gpu-unlock/assets
```

`make assets`는 해당 저장소에서 다음 작업을 연결합니다.

- `firmware/fetch_artifacts.py`: `NVIDIA-kernel-module-source-610.43.03.tar.xz`, `nvidia-utils-610.43.03-1-x86_64_v3.pkg.tar.zst` 다운로드 및 입력 해시 확인
- `firmware/generate_v67.py`: `v67-ga102-610.43.03.bin` 생성
- `firmware/extract_boot_assets.py`: `booter_load_ga102.bin`, `gsp_ga10x.bin`, `gsp_boot_ga102.bin`, `gsp_boot_desc_ga102.bin` 추출

기본 출력은 `~/.cache/gpu-unlock/assets/`입니다. 위 생성 명령은 **ASSETS만 준비**하며 USB·GPU는 변경하지 않습니다.

**중요:** 과거 `Makefile` 경로는 확인했지만, 현재 upstream 소스로 재생성한 5개 파일이 당시 성공한 SHA-256과 같은지는 별도 확인하지 않았습니다. 최신 upstream에서 빌드 내용이 달라졌다면 `verify-assets.py` 검사를 통과하지 못할 수 있습니다. 파일명 변경이나 해시 검사 우회는 하지 마세요.

## 4. 파일 배치

소유·사용 권한이 있는, 위 해시와 일치하는 5개 파일을 확보한 경우에만 USB(예: `F:` / WSL `/mnt/f`)에 다음과 같이 복사합니다.

```text
F:\\
├── EFI\\BOOT\\...
└── RTX3080-UNLOCK\\ASSETS\\
    ├── booter_load_ga102.bin
    ├── v67-ga102-610.43.03.bin
    ├── gsp_ga10x.bin
    ├── gsp_boot_ga102.bin
    └── gsp_boot_desc_ga102.bin
```

다음 명령은 **검사만 수행**합니다. GPU 접근·언락·USB 포맷·실행 마커 생성을 하지 않습니다.

```bash
# 저장소 최상위에서, USB가 F:일 때
python3 scripts/verify-assets.py /mnt/f/RTX3080-UNLOCK/ASSETS
```

로컬 `assets` 폴더에 먼저 준비했다면 다음처럼 검사할 수도 있습니다.

```bash
python3 scripts/verify-assets.py ./assets
```

`PASS: all 5 assets verified`가 출력되는 것이 자산 파일 동일성의 조건입니다. **이것은 펌웨어 실행의 안전성이나 다른 CPU/메인보드의 호환성을 검증하는 것이 아닙니다.**

## 5. ASSETS만 준비해도 완제품이 되지 않는 이유

Stage 21 구성은 `Stage15 BOOTX64.EFI` + `Stage20 RTX3080-UNLOCK.EFI` + `Stage21 RTX3080-STAGE19-ENGINE.EFI`가 필요합니다. 이 저장소에는 검증된 EFI 바이너리가 들어 있지 않습니다. [`BUILD_AND_DEPLOY_KO.md`](BUILD_AND_DEPLOY_KO.md)를 읽고, 당시 설치 스크립트가 이전 단계 파일과 고정 SHA-256을 요구한다는 점을 이해해야 합니다. `install-once.sh`는 **새 USB용 원클릭 설치기가 아닙니다**.

특히 `docs/USB-BOOT.md`는 **과거 Stage 12의 별도 구성**(내장 GPU 사용 및 대화형 UEFI)을 설명하며, 검증된 Stage 21의 Ryzen 5 3600 단독 GPU 부팅 절차와 다릅니다. 이를 Stage 21 설치법으로 사용하지 마세요.

## 6. 생성·검증 범위

- **확인됨:** 기존 `gpu-unlock` `Makefile`에서 `make assets` → NVIDIA 입력 다운로드 → V67 생성·부팅 자산 추출의 경로를 확인했습니다.
- **실행 필요:** 새 환경에서 재생성한 파일의 크기·SHA-256이 위 5개와 완전히 같은지 검사해야 합니다.
- **지원 범위:** 정확한 ASSETS 일치는 파일 동일성 검증일 뿐입니다. 다른 GPU·메인보드·CPU의 안전성이나 언락 성공을 보장하지 않습니다.
- NVIDIA 펌웨어 바이너리를 GitHub에 업로드하지 마세요.
