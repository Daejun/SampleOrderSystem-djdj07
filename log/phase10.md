# Phase 10 구현 완료 및 Verify 결과

새 기능 추가 없는 버그 수정 Phase다. 이 프로젝트의 마지막 Phase다.

- 관련 설계 문서: `docs/design/phase10.md`
- 관련 계획: `PLAN.md` Phase 10

## 구현 내용

| 파일 | 내용 |
|---|---|
| `src/main.cpp` | 최상단에 `#ifdef _WIN32` / `<windows.h>` 추가. `main()` 시작 시 `SetConsoleOutputCP(CP_UTF8)`(출력)와 `SetConsoleCP(CP_UTF8)`(입력) 호출 추가. `#ifdef _WIN32`로 감싸 이식성 유지 |

## Verify 결과

### 1. Test Verify — `scripts/build.sh`

```
$ bash scripts/build.sh
...
100% tests passed, 0 tests failed out of 70
```

결과: **통과 (70/70, 회귀 없음)**. `main()`을 거치지 않는 테스트 바이너리는 이번 변경의 영향을 받지 않는다는 설계 문서의 예상대로였다.

### 2. Compliance Verify — `docs/design/phase10.md` / `PLAN.md` Phase 10 요구사항 대조

| 요구사항 | 충족 여부 |
|---|---|
| 기존 70개 테스트 전체 회귀 없이 통과 | O (70/70) |
| `chcp 949` 강제 후 앱 실행 시 콘솔 코드페이지가 65001로 바뀌는지 확인 | O (아래 Demonstrability) |
| `main.cpp`에 코드페이지 고정 로직이 정확히 1곳에 존재 | O — `main()` 진입부 1곳, 다른 파일 변경 없음 |

### 3. Document Consistency Verify

구현 전 `CLAUDE.md`/`prd.md`/`PLAN.md`/`docs/design/phase10.md` 상호 참조 확인. 새 도메인 규칙과
무관한 순수 버그 수정이며 추가 충돌 없음.

### 4. Demonstrability — 간접 검증 (콘솔 코드페이지 변화)

설계 문서에서 명시한 대로, 이 버그는 콘솔이 바이트를 화면에 렌더링하는 방식의 문제라 파이프로 리다이렉트해
캡처하는 자동화 도구로는 mojibake 자체를 재현할 수 없다(리다이렉트된 스트림은 원본 UTF-8 바이트 그대로
캡처되어 항상 정상으로 보임). 대신 코드페이지 강제 변경 → 실행 → 재확인으로 API 호출의 실제 효과를
증명했다.

```
PS> chcp 949
Active code page: 949          # (강제로 949로 설정, 실제로는 콘솔 코드페이지가 949라 이 메시지 자체도 깨져 표시됨)

PS> echo 0 | build\src\sample_order_app.exe    # 수정된 앱 실행 (SetConsoleOutputCP(CP_UTF8) 내부 호출)

PS> chcp
Active code page: 65001        # <- 949였던 코드페이지가 앱 실행 후 65001로 바뀜. 정상적으로 렌더링됨.
```

`SetConsoleOutputCP`가 콘솔(창) 단위로 적용되고 프로세스 종료 후에도 유지되는 Windows 콘솔의 특성을
이용해, 949로 강제한 직후 실행 전/후 코드페이지 값을 비교함으로써 API 호출이 실제로 효과를 냈음을
간접적으로 증명했다.

재현 커맨드 (PowerShell):
```
chcp 949
build\src\sample_order_app.exe
chcp   # 65001로 바뀌어 있으면 정상
```

## 검증하지 못한 내용

- 실제 사용자 환경에서 한글이 시각적으로 올바르게 렌더링되는지는 **자동화 도구로 직접 관찰이 불가능**한
  영역이다(파이프 캡처는 코드페이지 문제를 우회하므로). 위의 간접 증거(코드페이지 값 변화)로 API 호출의
  효과를 확인했을 뿐, 사람이 직접 콘솔 창에서 눈으로 확인하는 최종 검증은 사용자 쪽에서 필요하다.
- 콘솔 폰트가 한글 글리프를 지원하지 않는 경우(레거시 래스터 폰트 등)는 설계 문서에 명시한 대로 이번
  Phase의 범위 밖이며 코드로 해결할 수 없다.

## 프로젝트 완료 메모

PLAN.md에 정의된 Phase 1~10(기능 Phase 1~8 + 클린업 Phase 9 + 버그 수정 Phase 10) 전체가 완료되었다.
최종 테스트 현황은 70/70 통과.
