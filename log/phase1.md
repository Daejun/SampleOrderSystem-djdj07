# Phase 1 구현 완료 및 Verify 결과

- 관련 설계 문서: `docs/design/phase1.md`
- 관련 계획: `PLAN.md` Phase 1
- 커밋: `30bcdc4` (feat: Phase 1 프로젝트 스캐폴딩 완성 (MVC 구조 + CMake 빌드))

## 구현 내용

| 파일 | 내용 |
|---|---|
| `CMakeLists.txt` | C++17, `nlohmann_json`(v3.11.3) FetchContent, `src`/`tests` 서브디렉토리 구성 |
| `src/persistence/JsonDocument.h/.cpp` | `json_crud` PoC의 `JsonDocument`를 `sampleorder` 네임스페이스로 이식 (atomic write: `.tmp` 파일에 쓴 뒤 `rename`) |
| `src/view/IView.h` | `mvc` PoC의 `IView`에서 `Item` 참조 제거, `showMessage`/`showError`만 남긴 메시지 전용 인터페이스 |
| `src/view/ConsoleView.h/.cpp` | `IView` 콘솔 구현체 |
| `src/controller/MainController.h/.cpp` | 메뉴 선택(1~6) 시 "미구현" 메시지 출력, `0` 입력 시 종료 플래그 설정, 그 외 입력은 오류 처리 |
| `src/main.cpp` | 메인 메뉴 출력 및 입력 루프 |
| `tests/PlaceholderTest.cpp` | `mvc` PoC 패턴의 placeholder 테스트 |
| `tests/CMakeLists.txt` | `googletest`(v1.15.2) FetchContent, `gtest_discover_tests` |
| `.gitignore` | `build/`, `data/`, 산출물 확장자 제외 |

## Verify 결과

### 1. Test Verify — `cmake --build` / `ctest`

```
$ cmake -S . -B build -G "MinGW Makefiles"
...
-- Configuring done (31.5s)
-- Generating done (0.3s)
-- Build files have been written to: C:/Users/User/semi/build

$ cmake --build build
[100%] Built target sample_order_app
[100%] Built target sample_order_tests
(gtest/gmock 관련 타겟 포함 전체 빌드 성공)

$ ctest --output-on-failure
Test project C:/Users/User/semi/build
    Start 1: Placeholder.BuildWorks
1/1 Test #1: Placeholder.BuildWorks ...........   Passed    0.01 sec
100% tests passed out of 1
```

결과: **통과**. 임의 완화나 우회 없이 실제 빌드·테스트 실행으로 확인.

### 2. Compliance Verify — `docs/design/phase1.md` / `PLAN.md` Phase 1 요구사항 대조

| 요구사항 | 충족 여부 |
|---|---|
| MVC 디렉토리 구조(`model/view/controller`) 확립 | O (`model`은 Phase 2에서 채울 빈 디렉토리로 남김 — 설계상 정상) |
| CMake 빌드 체계로 빌드/실행만 되는 콘솔 앱 | O |
| `JsonDocument` 이식(도메인 미연결) | O |
| 메뉴 스텁(선택지 출력, 기능 미구현) | O |
| Placeholder 테스트로 `ctest` 통과 | O |

### 3. Document Consistency Verify

구현 시작 전 `CLAUDE.md`/`prd.md`/`PLAN.md`/`docs/design/phase1.md` 간 충돌 여부 확인함. 이전에 발견된 PLAN.md-CLAUDE.md 간 PoC 활용 방침 불일치는 별도로 확인받아 PLAN.md에 반영 완료된 상태였음(커밋 `0bfdb43`). 이번 구현 범위에서는 추가 충돌 없음.

### 4. Demonstrability — 콘솔 실행 로그

```
$ printf "1\n5\n9\n0\n" | ./build/src/sample_order_app.exe

=== 반도체 시료 생산주문관리 시스템 ===
[1] 시료 관리
[2] 시료 주문
[3] 주문 승인/거절
[4] 모니터링
[5] 생산 라인 조회
[6] 출고 처리
[0] 종료
선택 > [시료 관리] 미구현입니다.

=== 반도체 시료 생산주문관리 시스템 ===
...
선택 > [생산 라인 조회] 미구현입니다.

=== 반도체 시료 생산주문관리 시스템 ===
...
선택 > Error: 알 수 없는 선택입니다: 9

=== 반도체 시료 생산주문관리 시스템 ===
...
선택 >
```

재현 커맨드:
```
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
printf "1\n5\n9\n0\n" | ./build/src/sample_order_app.exe
```

## 검증하지 못한 내용

- `JsonDocument`는 이식만 했을 뿐 실제 파일 저장/로드 라운드트립 테스트는 작성하지 않음 (설계 범위상 Phase 2에서 `SampleRepository`가 최초로 연결하며 그때 테스트 예정).
- Windows 외 플랫폼(Linux/macOS)에서의 빌드는 시도하지 않음 — 현재 개발 환경(Windows + MinGW)에서만 확인.
