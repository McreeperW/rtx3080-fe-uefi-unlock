# Stage 26 — 독립 소스 빌드 및 검증

Stage 26은 Stage 21과 같은 계열의 UEFI 엔진이지만, **독립 복제된 `fe/stage26/engine/`** 소스에서 빌드합니다. `fe/stage21/engine/`을 빌드 과정에서 수정하거나 참조하지 않습니다(정책 구분 검사는 Stage21 파일을 읽기만 함).

## 1. 두 정책 확인

```bash
grep -n 'a.known_observation && a.gfw_idle' fe/stage21/engine/app.c
grep -n 'STAGE26 SS0 snapshot' fe/stage26/engine/app.c
```

첫 명령은 Stage 21의 초기 SS0 화이트리스트 검사를, 둘째 명령은 Stage 26의 관측 전용 로그를 보여줘야 합니다.

## 2. 독립 빌드 및 테스트

```bash
bash fe/stage21/build-engine.sh
bash fe/stage21/tests/run.sh
bash fe/stage26/build.sh
```

Stage 26 빌더는 **Stage 21 빌더를 호출하지 않습니다.** `fe/stage26/engine` 아래 모든 소스와 EDK2 헤더를 이용해 다시 컴파일하고, Stage 26 모의 테스트까지 실행합니다. Stage21과 Stage26 실행 파일은 각기 다른 `out/` 폴더에 남습니다.

## 3. 파일명 및 해시

| 결과 파일 | 빌드 정책 |
|---|---|
| `fe/stage21/out/RTX3080-STAGE19-ENGINE.EFI` | Stage 21, 초기 SS0 네 값의 화이트리스트 |
| `fe/stage26/out/RTX3080-STAGE19-ENGINE.EFI` | Stage 26, 초기 SS0 관측 전용. Stage 20 브리지 호환 이름 |
| `fe/stage26/out/RTX3080-STAGE26-ENGINE.EFI` | 바로 위 Stage 26 EFI의 동일 바이트 복사본, 구별용 |

```bash
sha256sum fe/stage21/out/RTX3080-STAGE19-ENGINE.EFI \
          fe/stage26/out/RTX3080-STAGE19-ENGINE.EFI \
          fe/stage26/out/RTX3080-STAGE26-ENGINE.EFI
```

Stage 26 두 파일은 **동일 SHA-256**이어야 하고, Stage 21과 Stage 26의 EFI는 정책이 다르므로 **서로 달라야** 합니다. 컴파일 절대 경로에 따른 문자열 등으로 각 이전 실기 EFI와의 해시는 다를 수 있습니다.

## 4. 설치와 성공 범위

- 이 절차는 **소스 빌드·호스트 테스트 전용**입니다. F:/G: USB나 Windows ESP, 자동 언락 잠금과 마커를 건드리지 않습니다.
- 두 엔진의 USB 파일명은 동일하게 요구되므로, 실기에서 버전을 바꿀 때는 반드시 별도 백업과 해시 검증이 필요합니다. 기존 `fe/stage21/install-once.sh`를 Stage26 범용 설치기로 사용하지 마세요.
- Stage 26 첫 성공 측정: SS0 초기 `0x50340010` → 언락 후 `0x88888888`; FP16 +84.60%, BF16 +85.92%. **단일 시스템 한 번의 실기 성공**으로 반복 콜드부팅 안정성·다른 카드에서의 안전성을 보장하지 않습니다.
