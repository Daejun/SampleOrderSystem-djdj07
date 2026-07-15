# Phase 7 구현 완료 및 Verify 결과

- 관련 설계 문서: `docs/design/phase7.md`
- 관련 계획: `PLAN.md` Phase 7

## 구현 내용

| 파일 | 내용 |
|---|---|
| `src/controller/SubMenu.h/.cpp` | 신규. `submenu::run(printMenu, view, handlers)` 공통 헬퍼 — 서브메뉴 루프 중복(Rule of Three) 제거 |
| `src/model/MonitoringService.h/.cpp` | 신규. `OrderCountSummary`(REJECTED 제외 상태별 집계), `InventoryLevel`/`SampleInventoryStatus`(재고 3구간 판정) |
| `src/controller/MonitoringController.h/.cpp` | 신규. `showOrderSummary`/`showInventoryStatus` |
| `src/view/IView.h`, `ConsoleView.h/.cpp` | `showOrderCountSummary`/`showInventoryStatus` 추가 |
| `src/controller/MainController.h/.cpp` | 기존 4개 서브메뉴(`runSampleMenu`/`runOrderMenu`/`runApprovalMenu`/`runReleaseMenu`)를 `submenu::run()` 호출로 리팩터링, "4" 선택 시 `runMonitoringMenu()` 신설, 생성자에 `MonitoringController&` 추가 |
| `src/main.cpp` | `MonitoringService`/`MonitoringController` 생성·연결 |
| `tests/MonitoringServiceTest.cpp` | 신규. REJECTED 제외 집계, 재고 판정 3구간(고갈/경계값 포함 여유/부족), CONFIRMED·PRODUCING 등 비교 대상 제외 확인 |
| `tests/MonitoringControllerTest.cpp` | 신규. View 위임 확인 |
| `tests/StubView.h` | `showOrderCountSummary`/`showInventoryStatus` 추가 |
| `tc.md` | 1부 인벤토리에 신규 테스트 8개 반영 |
| `PLAN.md` | Phase 7 구현 완료 노트 추가 |

## 사람 확인 사항 반영

- 재고 판정 기준(고갈 최우선, 경계값 포함 여유, RESERVED만 비교 대상)과 서브메뉴 공통화 방향 모두
  `docs/design/phase7.md`에서 이미 "사람 확인 완료"로 명시되어 있어 별도 재확인 없이 그대로 구현했다.

## Verify 결과

### 1. Test Verify — `scripts/build.sh`

구현 중 `MonitoringServiceTest.OrderCountSummaryExcludesRejected`가 1회 실패했다. Phase 6에서와
동일한 패턴의 테스트 설계 실수였다 — 같은 시료에 대해 앞서 대량 주문(10000개)을 승인하며 재고가 0으로
소진되었는데, 뒤이은 "출고까지 완료되는" 케이스에서 재고를 다시 채우지 않아 그 주문도 재고 부족으로
PRODUCING에 머물러(RELEASE로 전환되지 못해) 집계 기댓값과 어긋났다. 테스트에 `setStock` 재호출과
`ASSERT_EQ`/`ASSERT_TRUE`로 각 전이 단계의 상태를 명시적으로 확인하는 코드를 추가해 수정했다.

```
$ bash scripts/build.sh
...
[100%] Built target sample_order_tests
...
Test project C:/Users/User/semi/build
 (68개 테스트 전체)

100% tests passed, 0 tests failed out of 68
```

결과: **통과 (68/68, Phase 1~6의 기존 60개 포함 회귀 없음 — 서브메뉴 리팩터링에도 불구하고 기존
`SampleControllerTest`/`OrderControllerTest` 전체 그대로 통과)**. 임의 완화/우회 없음.

### 2. Compliance Verify — `docs/design/phase7.md` / `PLAN.md` Phase 7 요구사항 대조

| 요구사항 | 충족 여부 |
|---|---|
| 상태별 주문 건수 집계, REJECTED 제외 | O (`OrderCountSummaryExcludesRejected`) |
| 재고 판정 3구간 경계값(0, 부족 임계, 여유) | O (`InventoryStatusDepletedWhenStockIsZeroRegardlessOfReserved`, `InventoryStatusPlentyWhenStockEqualsReservedSum`, `InventoryStatusLowWhenStockBelowReservedSum`) |
| PRODUCING/CONFIRMED/RELEASE는 재고 판정 비교 대상 제외 | O (`InventoryStatusIgnoresNonReservedOrdersInComparison`) |
| 서브메뉴 헬퍼 추출 후 기존 5개 메뉴 전체 회귀 없음 | O (`SampleControllerTest`/`OrderControllerTest` 전체 통과) |
| `ctest` 전체 통과 | O (68/68) |
| 콘솔 실행 로그로 "주문 혼합 상태 → 모니터링(주문량/재고량)" 재현 | O (아래 Demonstrability) |

### 3. Document Consistency Verify

구현 전 `CLAUDE.md`/`prd.md`/`PLAN.md`/`docs/design/phase7.md` 상호 참조 확인. Phase 6 코드 리뷰에서
지적된 서브메뉴 중복(Rule of Three)을 설계·구현 모두에 반영했다. 추가 충돌 없음.

### 4. Demonstrability — 콘솔 실행 로그

```
$ ./build/src/sample_order_app.exe
...
선택 > 1   # 시료 등록 (재고 0으로 시작)
시료 등록 완료: S-001

선택 > 2   # 주문1(50ea) 접수
주문 접수 완료: ORD-20260715-0001
선택 > 2   # 주문2(30ea) 접수
주문 접수 완료: ORD-20260715-0002

선택 > 3   # 주문1 승인 -> 재고 0 < 50 -> PRODUCING
주문 승인 완료 (재고 부족, 생산 대기): ORD-20260715-0001

선택 > 4   # 모니터링 -> 주문량 확인
RESERVED  1건    (주문2, 아직 미승인)
CONFIRMED 0건
PRODUCING 1건    (주문1)
RELEASE   0건

선택 > 2   # 모니터링 -> 재고량 확인
[S-001] 실리콘 재고 0 ea (RESERVED 합 30 ea) - 고갈
```

재고가 0이면 RESERVED 합(30ea, 주문2)이 존재해도 "부족"이 아니라 **"고갈"로 최우선 판정**됨을
직접 확인했다(설계 문서의 판정 순서 그대로).

재현 커맨드:
```
bash scripts/build.sh
./build/src/sample_order_app.exe
```

## 검증하지 못한 내용

- `submenu::run()` 자체(및 각 서브메뉴 handler 내부의 `std::cin` 입력 파싱)에 대한 자동화 단위 테스트는
  작성하지 않았다 — 설계 문서에서 이번 리팩토링의 목적을 "테스트 가능성 확보가 아니라 중복 제거(DRY)"로
  명시적으로 한정했으므로, 회귀 확인은 기존 Controller 테스트 통과 여부로만 검증했다.
- `MainController::runMonitoringMenu()`의 `std::cin` 기반 입력 루프 자체에 대한 자동화 테스트는 작성하지
  않음 (콘솔 실행 로그로만 확인) — 이전 Phase와 동일한 미검증 범위.
