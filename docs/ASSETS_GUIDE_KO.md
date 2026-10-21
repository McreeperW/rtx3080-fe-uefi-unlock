# Stage 21용 NVIDIA 펌웨어 ASSETS 준비 및 검증

> **상태: 필요 파일과 동일성 검사 방법은 확인됐지만, 신규 사용자를 위한 5개 자산의 추출·생성 절차는 아직 끝까지 재현·검증되지 않았습니다.** 이 문서는 설치 안내를 가장하지 않습니다. 다른 GPU나 보드에서 실행하면 화면 출력 중단·강제 전원 종료 등이 발생할 수 있습니다.

## 1. 왜 Git 저장소에 없는가?

이 프로젝트는 실제 NVIDIA 드라이버/펌웨어에서 준비한 바이너리와, 관련 파생 생성물을 사용합니다. 라이선스·저작권 문제가 있을 수 있으므로 Git 저장소와 릴리스에 자산 바이너리를 동봉하지 않습니다. **이 파일은 소스 코드만 빌드해서 자동으로 생기지 않습니다.**

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

## 3. 원본 후보와 중요한 제약

- `gsp_ga10x.bin`: 테스트에서 사용한 파일은 **NVIDIA 610.43.03 계열 GSP 펌웨어**입니다. 시스템에 설치된 현재 Windows NVIDIA 610.88 드라이버가 동일한 펌웨어 파일을 제공한다고 **가정하면 안 됩니다**. 정확한 바이트 크기와 SHA-256이 같아야 합니다.
- `booter_load_ga102.bin`, `gsp_boot_ga102.bin`, `gsp_boot_desc_ga102.bin`: NVIDIA 드라이버/펌웨어 입력을 가공·추출하여 얻는 유형의 자산입니다. **공식 드라이버 배포 파일에서 해당 이름 그대로 발견된다는 뜻이 아닙니다**. 이 저장소에는 실험 당시의 추출 명령과 전체 재현 기록이 없습니다.
- `v67-ga102-610.43.03.bin`: GPU 언락에 사용하는 **V67 생성 페이로드**이며, 일반 NVIDIA 드라이버 설치 폴더에 같은 이름으로 포함된다고 가정하면 안 됩니다. 생성 버전과 크기·해시를 반드시 맞춰야 합니다.

기술적 참고 자료: [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock)의 `firmware/` 소스 및 당시 출처 기록을 확인하세요. 해당 Linux용 프로젝트의 최신 산출물을 가져온다고 이 Windows UEFI Stage 21의 정확한 5개 SHA-256과 일치한다는 **보장은 없습니다**. 이 저장소에는 Windows Stage 21용으로 검증된, 5개 파일 자동 다운로드·추출 스크립트가 없습니다. 잘못된 바이너리를 이름만 맞춰 넣거나 해시 검사를 무시하지 마세요.

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

## 6. 재현 기여가 필요한 부분

각 파일의 원본 NVIDIA 배포물 버전과 SHA-256, 추출/생성 스크립트, 도구 버전, 생성 파일의 상기 SHA-256 일치를 확인하는 공개 재현 기록이 아직 필요합니다. 그 과정이 끝나기 전에는 타인에게 '클론 후 실행 가능' 또는 '범용 AM4 지원'이라고 안내하지 마세요. 사용자가 소유·이용 허가를 받지 않은 NVIDIA 바이너리를 저장소나 이슈에 올리지 마세요.
