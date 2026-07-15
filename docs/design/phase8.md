# Phase 8 설계: 메인 메뉴 통합 및 E2E

`PLAN.md`의 Phase 8 목표(메인 메뉴 요약 정보 표시, 전체 시나리오 E2E 검증)에 대한 상세 설계. 이 프로젝트의
마지막 Phase이며, Phase 1~7에서 이미 만든 도메인 로직을 새로 만들지 않고 **재사용·연결**하는 데 집중한다.

## 확정된 결정 사항 (합리적 해석 — 사람 확인 없이 진행, 근거 명시)

`prd.md` §4.1은 "등록 시료 수, 총 재고, 전체 주문 수, 생산라인 대기 건수"라고만 되어 있고 §4.5(모니터링)와
달리 REJECTED 제외를 명시하지 않는다. 아래처럼 해석한다 — 다른 해석을 원하면 조정 가능하다.

| 항목 | 정의 | 근거 |
|---|---|---|
| 등록 시료 수 | `sampleRepository.list().size()` | 그대로 |
| 총 재고 | 모든 시료의 `stock` 합 | 그대로 |
| 전체 주문 수 | `orderRepository.list().size()` (REJECTED **포함**, 모든 상태) | §4.1은 REJECTED 제외를 명시하지 않음. §4.5(모니터링)의 "무효 주문 제외"는 상태별 세부 집계에 대한 규칙이지 "전체" 카운트에 대한 규칙이 아니라고 해석 |
| 생산라인 대기 건수 | `MonitoringService::orderCountSummary().producing` (대기 중+활성 생산 모두 포함) | Phase 7에서 이미 "PRODUCING 건수"를 집계하고 있어 `ProductionQueue`를 별도로 다시 조회하지 않고 재사용 |

## `MonitoringService` 확장 (새 클래스 신설 없음)

메인 메뉴 요약도 결국 Sample/Order 집계이므로, Phase 7에서 만든 `MonitoringService`에 메서드 하나만
추가한다 (이미 `SampleRepository&`/`OrderRepository&`를 들고 있어 그대로 재사용 가능).

```cpp
// src/model/MonitoringService.h — 추가
struct MainMenuSummary {
    int sampleCount;
    int totalStock;
    int totalOrderCount;   // REJECTED 포함 전체
    int producingCount;    // 생산 대기+활성 생산 합계
};

MainMenuSummary mainMenuSummary() const;
```

## 메인 메뉴 요약 갱신 시점

메인 메뉴는 `main.cpp`의 루프에서 **매 반복마다** 출력된다. 생산 완료 판정(`ProductionQueue::advance()`)은
지금까지 (1) 앱 시작 시 1회, (2) "생산 라인 조회" 메뉴 진입 시에만 호출되고 있었다. 사용자가 "생산 라인
조회"를 들르지 않고 다른 메뉴만 오가면 메인 메뉴 요약의 "생산라인 대기 건수"가 실제로 완료된 생산을
반영하지 못한 채 오래된 값을 보여줄 수 있다.

이번 Phase에서 **메인 메뉴를 출력하기 직전에도 `productionQueue.advance()`를 호출**하도록 `main.cpp`를
수정한다. `advance()`는 한 번 호출에 최대 한 단계만 진행하고 저장 여부와 무관하게 항상 안전하게 반복
호출 가능하도록 이미 설계되어 있으므로(Phase 5), 매 루프마다 호출해도 부작용이 없다.

## View

```cpp
// src/view/IView.h — 추가
virtual void showMainMenu(const MainMenuSummary& summary) = 0;
```

`ConsoleView::showMainMenu()`가 기존 `main.cpp`의 `printMenu()` 텍스트(메뉴 항목 1~6, 0)에 요약 정보
줄을 추가해 출력한다. `main.cpp`의 `printMenu()` 자유 함수는 제거하고 `view.showMainMenu(summary)` 호출로
대체한다 (기존 서브메뉴들은 `IView`를 거치지 않고 `std::printf`를 직접 썼지만, 최상위 메뉴는 요약 데이터를
다뤄야 하므로 다른 도메인 표시와 동일하게 `IView`를 통하는 것이 일관적이다).

## E2E 테스트

지금까지 콘솔 입력 루프(`MainController`의 각 `runXxxMenu`) 자체는 `std::cin` 의존 때문에 자동화
테스트 대상이 아니었다(Phase 2부터 반복적으로 명시된 미검증 범위). 이번 Phase에서는 **Controller
아래 계층(Repository/Service)을 직접 호출**해 전체 시나리오를 자동화된 회귀 테스트로 남긴다 — 콘솔
루프를 모킹하지 않고, 이미 각 Phase에서 검증된 컴포넌트들을 실제로 연결해 처음부터 끝까지 통과시킨다.

```cpp
// tests/EndToEndTest.cpp (신규)
// 시나리오: 시료 등록(재고 0) -> 주문 접수(재고 부족량) -> 승인(PRODUCING 전환)
//          -> 생산 큐 advance()(고정 Clock으로 시간 경과 시뮬레이션, 생산 시작+완료)
//          -> CONFIRMED 확인 -> 출고(release) -> RELEASE 확인
//          -> MonitoringService로 최종 집계(전체 요약, 재고 상태) 확인
TEST(EndToEndTest, FullOrderJourneyFromReservationToRelease) { ... }
```

- 고정(가변) Clock 주입은 `ProductionQueueTest`에서 이미 쓰던 패턴을 그대로 재사용한다.
- 이 테스트는 "콘솔 로그로 재현"이라는 Demonstrability 요건을 대체하지 않는다 — PLAN.md 완료 기준의
  "콘솔 상에서 End-to-End로 재현"은 실제 `sample_order_app.exe` 실행 로그로 별도 제공하고, 이 E2E 테스트는
  그 시나리오를 **회귀 방지용으로 자동화**한 것이다.

## 디렉토리/파일 변경 요약

| 파일 | 변경 |
|---|---|
| `src/model/MonitoringService.h/.cpp` | `MainMenuSummary`/`mainMenuSummary()` 추가 |
| `src/view/IView.h`, `ConsoleView.h/.cpp` | `showMainMenu` 추가 |
| `src/main.cpp` | `printMenu()` 자유 함수 제거, `monitoringService.mainMenuSummary()` + `view.showMainMenu()`로 대체, 루프마다 `productionQueue.advance()` 호출 추가 |
| `tests/MonitoringServiceTest.cpp` | `mainMenuSummary()` 케이스 추가 (REJECTED 포함 전체 카운트, producing 재사용 확인) |
| `tests/EndToEndTest.cpp` | 신규. 전체 시나리오 자동화 회귀 테스트 |
| `tc.md` | 1부 인벤토리에 신규 테스트 반영 |

## Phase 8에서 하지 않는 것

- 새 도메인 로직/Controller 추가 (이미 Phase 1~7에서 모두 구현됨)
- 콘솔 입력 루프(`MainController`) 자체의 자동화 테스트 (기존과 동일한 미검증 범위 유지)
- prd.md에 없는 대시보드/통계 확장

## 완료 기준

- 메인 메뉴에 등록 시료 수/총 재고/전체 주문 수(REJECTED 포함)/생산라인 대기 건수가 정확히 표시되는지 테스트
- 메인 메뉴 출력 시마다 `advance()`가 호출되어 생산 완료가 지연 없이 반영되는지 확인
- `EndToEndTest`로 "접수 → 승인(부족) → 생산 시작 → 완료(CONFIRMED) → 출고(RELEASE)" 전체 흐름이 한 번에 통과하는지 검증
- `ctest` 전체(기존 68개 + 신규) 통과
- 콘솔 실행 로그로 위 전체 흐름과 메인 메뉴 요약 정보 변화(주문 접수 전/후, 생산 완료 전/후)를 함께 재현 (Demonstrability)
