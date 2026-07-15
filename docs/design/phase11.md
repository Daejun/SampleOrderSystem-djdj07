# Phase 11 설계: 테스트 커버리지 보강

`PLAN.md`의 Phase 11 목표(발견된 3가지 테스트 공백 보강)에 대한 상세 설계. 새 기능·동작 변경 없이
**테스트만 추가**하는 Phase다.

## 배경

`tc.md` 충분성 점검(코드 대조) 중 발견된 3가지 공백:

1. `ConsoleView`(실제 `IView` 구현체)에 대한 자동화 테스트가 전혀 없음 — 모든 Controller 테스트는
   `StubView`로 "위임 여부"만 확인하고, 실제 `printf` 포맷 문자열은 각 Phase `log/phaseN.md`의 수동
   콘솔 로그로만 검증되어 왔다.
2. `SampleRepository::search()`가 실제로는 `id`/`name`/`avgProductionTimeMinutes`/`yield` 4개 필드를
   비교하는데(Phase 2 설계), 기존 테스트(`SearchMatchesPartialCaseInsensitive`)는 `id`/`name`만 검증한다.
3. `OrderController::approveOrder()`는 승인 결과가 PRODUCING이면 다른 메시지("재고 부족, 생산 대기")를
   보여주는 분기가 있는데, 기존 Controller 테스트(`ApproveOrderShowsSuccessMessage`)는 재고 충분
   (CONFIRMED) 케이스만 재고 500으로 세팅해 검증한다.

## 1. `tests/ConsoleViewTest.cpp` 신규 (`mvc` PoC 패턴 이식)

`mvc/tests/ConsoleViewTest.cpp`의 `testing::internal::CaptureStdout()`/`GetCapturedStdout()` 패턴을
그대로 가져와 semi의 `ConsoleView` 10개 메서드 전부를 검증한다. `find(...) != npos`로 핵심 값이 출력에
포함되는지만 확인하고, 전체 문자열 포맷을 하드코딩해 비교하지 않는다(사소한 문구 수정에 테스트가 매번
깨지는 것을 방지 — `mvc` PoC와 동일한 관용).

| 테스트 | 확인 내용 |
|---|---|
| `ShowMessagePrintsMessage` | 메시지 텍스트가 그대로 출력됨 |
| `ShowErrorPrintsErrorPrefixedMessage` | `Error:` 접두사 + 메시지 |
| `ShowSamplesPrintsEachSample` | 여러 시료의 id/name/재고 값이 모두 출력됨 |
| `ShowSamplesPrintsMessageWhenEmpty` | 빈 목록 시 안내 문구("등록된 시료가 없습니다") |
| `ShowSamplePrintsSampleDetail` | 단일 시료의 생산시간/수율/재고 값 출력 |
| `ShowOrdersPrintsEachOrder` | 여러 주문의 주문번호/수량/상태 출력 |
| `ShowOrdersPrintsMessageWhenEmpty` | 빈 목록 시 안내 문구("등록된 주문이 없습니다") |
| `ShowOrderPrintsOrderDetail` | 단일 주문 상세 출력 |
| `ShowProductionStatusPrintsActiveAndWaiting` | 활성 생산 + 대기 큐가 모두 있을 때 |
| `ShowProductionStatusPrintsNoActiveMessageWhenIdle` | 활성 생산 없을 때 안내 문구 |
| `ShowProductionStatusPrintsNoWaitingMessageWhenQueueEmpty` | 대기 큐 없을 때 안내 문구 |
| `ShowOrderCountSummaryPrintsAllFourCounts` | RESERVED/CONFIRMED/PRODUCING/RELEASE 4개 건수 모두 출력(REJECTED 필드 자체가 없음을 형태로 확인) |
| `ShowInventoryStatusPrintsLevelForEachSample` | 시료별 재고·RESERVED 합·판정(여유/부족/고갈) 문자열 출력 |
| `ShowInventoryStatusPrintsMessageWhenEmpty` | 등록된 시료 없을 때 안내 문구 |
| `ShowMainMenuPrintsSummaryAndMenuItems` | 요약 4개 수치 + 메뉴 항목(`[1]`~`[6]`, `[0]`)이 함께 출력됨 |

## 2. `SampleRepositoryTest.cpp`에 수치형 필드 검색 케이스 추가

```cpp
TEST(SampleRepositoryTest, SearchMatchesByProductionTimeAndYield) {
    // avgProductionTimeMinutes 값 일부로 검색 -> 매치
    // yield 값 일부로 검색 -> 매치
    // 두 시료의 값이 겹치지 않도록 구성해 교차 매치 오탐 없는지도 함께 확인
}
```

기존 `SearchMatchesPartialCaseInsensitive`(id/name)는 그대로 두고, 이 테스트를 별도로 추가해 4개 필드
전부가 실제로 검증되도록 한다.

## 3. `OrderControllerTest.cpp`에 PRODUCING 분기 메시지 케이스 추가

```cpp
TEST(OrderControllerTest, ApproveOrderWithInsufficientStockShowsProductionWaitingMessage) {
    // 재고를 주문 수량보다 적게 설정한 뒤 승인 -> PRODUCING 전환
    // view.lastMessage에 "생산 대기"(또는 동일 취지 문구)가 포함되는지 확인
    // 기존 ApproveOrderShowsSuccessMessage(충분 케이스)와 대비되는 케이스
}
```

## 4. `gcov` 커버리지 계측 도입 (사람 확인 완료)

이 프로젝트는 이미 WinGet mingw64(GCC) 툴체인으로 빌드하고 있고, `gcov`는 GCC에 기본 포함되어 있어
**새 의존성 없이** 바로 쓸 수 있다. 리포트를 보기 좋게 가공하는 `gcovr`/`lcov` 같은 추가 도구는 이번
Phase에서 도입하지 않는다 — 원본 `gcov` 텍스트 리포트만으로 "이번에 보강한 3개 영역의 커버리지가
실제로 올랐는지" 확인하기에 충분하다고 판단했다.

### CMake 옵션

```cmake
# 최상위 CMakeLists.txt에 추가
option(ENABLE_COVERAGE "Enable gcov coverage instrumentation" OFF)
if(ENABLE_COVERAGE)
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()
```

`ENABLE_COVERAGE`는 기본 OFF로 두어, 평소 `scripts/build.sh`로 하는 빌드에는 계측 오버헤드가 전혀
영향을 주지 않는다. `-O0`은 커버리지 라인 매핑이 최적화로 뒤섞이지 않도록 하기 위함이다.

### `scripts/coverage.sh` (신규)

`scripts/build.sh`와 별도의 빌드 디렉토리(`build-coverage/`)를 사용해, 평소 빌드 산출물과 섞이지 않게
한다.

```bash
#!/usr/bin/env bash
set -euo pipefail

MINGW_BIN="/c/Users/User/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin"
export PATH="$MINGW_BIN:$PATH"

cd "$(dirname "$0")/.."

cmake -S . -B build-coverage -G "MinGW Makefiles" -DENABLE_COVERAGE=ON
cmake --build build-coverage
ctest --test-dir build-coverage --output-on-failure

cd build-coverage
# .gcda가 생성된 각 디렉토리를 찾아 -o로 지정, 소스 상대경로 유지(-p)로 gcov 실행
find . -name '*.gcda' -exec dirname {} \; | sort -u | while read -r dir; do
    gcov -p -o "$dir" "$dir"/*.gcda >/dev/null
done
echo "커버리지 리포트(.gcov)가 build-coverage/ 에 생성되었습니다."
```

- `.gitignore`에 `build-coverage/`도 추가한다(`build/`와 동일하게 산출물 디렉토리이므로).
- 실행 후 `grep -E "model/(SampleRepository|OrderRepository)\.cpp|view/ConsoleView\.cpp"` 등으로 관심
  파일의 `.gcov`만 골라 라인 커버리지(파일 상단의 `Lines executed:XX.XX% of N`) 비교한다.

### 확인 방법

- 보강 전(이번 Phase 테스트 추가 전 커밋)과 보강 후 각각 `scripts/coverage.sh`를 실행해
  `src/view/ConsoleView.cpp.gcov`, `src/model/SampleRepository.cpp.gcov`,
  `src/controller/OrderController.cpp.gcov`의 `Lines executed:` 비율을 비교한다.
- `ConsoleView.cpp`는 보강 전 0%에 가까운 커버리지(간접 호출 없음)였다가, `ConsoleViewTest.cpp` 추가 후
  대부분의 라인이 실행되어야 한다.

## 디렉토리/파일 변경 요약

| 파일 | 변경 |
|---|---|
| `tests/ConsoleViewTest.cpp` | 신규 |
| `tests/SampleRepositoryTest.cpp` | `SearchMatchesByProductionTimeAndYield` 추가 |
| `tests/OrderControllerTest.cpp` | `ApproveOrderWithInsufficientStockShowsProductionWaitingMessage` 추가 |
| `tests/CMakeLists.txt` | `ConsoleViewTest.cpp` 빌드 대상 추가 |
| `tc.md` | 1부 인벤토리에 신규 테스트 반영 |
| `CMakeLists.txt` | `ENABLE_COVERAGE` 옵션 추가 |
| `scripts/coverage.sh` | 신규 |
| `.gitignore` | `build-coverage/` 추가 |

## Phase 11에서 하지 않는 것

- `ConsoleView`/`SampleRepository`/`OrderController`의 실제 로직 변경 (순수 테스트 추가)
- `MainController`의 `std::cin` 입력 루프 자동화 (여전히 별도 미검증 범위로 유지)
- `gcovr`/`lcov` 등 HTML 리포트 도구 도입 (사람이 원본 `gcov` 텍스트 리포트로 충분하다고 확인)
- `SubMenu::run()` 자체에 대한 테스트 (Phase 7에서 이미 "목적은 DRY, 테스트 가능성 아님"으로 범위 확정)

## 완료 기준

- `ConsoleViewTest`의 15개 테스트 전부 통과
- `SampleRepositoryTest.SearchMatchesByProductionTimeAndYield` 통과
- `OrderControllerTest.ApproveOrderWithInsufficientStockShowsProductionWaitingMessage` 통과
- 기존 70개 테스트 전체 회귀 없음
- `tc.md` 1부 인벤토리가 신규 테스트를 모두 반영해 실제 코드와 다시 1:1 일치
- `scripts/coverage.sh` 실행이 성공하고 `.gcov` 리포트가 생성됨
- 보강 전/후 비교에서 `ConsoleView.cpp`/`SampleRepository.cpp`/`OrderController.cpp`의 라인 커버리지가 실제로 상승했음을 확인
