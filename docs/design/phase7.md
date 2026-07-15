# Phase 7 설계: 모니터링

`PLAN.md`의 Phase 7 목표(주문량/재고량 모니터링)에 대한 상세 설계. Phase 6 코드 리뷰에서 발견되어
`PLAN.md`에 반영된 서브메뉴 루프 중복(Rule of Three)을 이번 Phase에서 해소한다.

## 확정된 결정 사항 (사람 확인 완료)

### 1. 재고 상태(여유/부족/고갈) 판정 기준

시료별로 아래 순서로 판정한다.

```
1. stock == 0          → "고갈" (주문 존재 여부와 무관, 최우선 판정)
2. stock >= RESERVED 합 → "여유" (경계값 포함 — 재고==RESERVED 합이면 "여유")
3. 그 외                → "부족"
```

`RESERVED 합`은 해당 시료를 대상으로 하는 **RESERVED 상태 주문의 quantity 합**만 사용한다.
PRODUCING/CONFIRMED/RELEASE 주문은 비교 대상에서 제외한다 — 이미 승인 시점에 "재고 충분/부족" 판정이
끝나 재고에 반영되었거나(Phase 4) 생산 큐로 넘어간 건(Phase 5)이므로, 여기서 다시 세면 중복 계산이다.
경계값(재고==RESERVED 합)을 "여유"로 두는 것은 Phase 4의 `ApproveExactStockMatchConfirmsWithoutShortage`
(재고==주문량이면 "충분" 분기)와 방향을 통일하기 위함이다.

### 2. 서브메뉴 루프 공통화 (Phase 6 코드 리뷰 반영, Rule of Three)

`MainController`의 기존 4개 서브메뉴 루프(`runSampleMenu`/`runOrderMenu`/`runApprovalMenu`/`runReleaseMenu`)가
"메뉴 출력 → `getline` → 문자열 분기 → `"0"`이면 종료 → 그 외 오류" 구조를 반복하고 있었다. 이번 Phase에서
신설할 모니터링 서브메뉴까지 포함하면 5번째가 되므로, 공통 헬퍼로 추출한다.

```cpp
// src/controller/SubMenu.h (신규, 도메인 비종속 순수 헬퍼)
namespace submenu {

using Handler = std::function<void()>;

// printMenu 출력 후 한 줄 입력을 받아 handlers에서 찾아 실행한다.
// "0"은 handlers에 등록하지 않아도 항상 종료로 처리한다. handlers에 없는 입력이면 view.showError.
// std::cin이 EOF(예: 파이프 입력 종료)면 즉시 종료한다.
void run(const std::function<void()>& printMenu, IView& view, const std::map<std::string, Handler>& handlers);

} // namespace submenu
```

- 각 메뉴 항목의 세부 입력(등록 시 4개 필드 등)은 여전히 handler 람다 내부에서 `std::cin`으로 직접
  읽는다 — 이 부분은 이번 리팩토링 범위가 아니다(Phase 2~3에서 이미 인지하고 있는 "입력 파싱과 I/O
  결합" 미검증 범위를 그대로 유지, Scope Creep 방지).
- `submenu::run()` 자체도 내부에서 `std::cin`을 직접 사용하므로 여전히 자동화 단위 테스트 대상은
  아니다 — 이번 리팩토링의 목적은 테스트 가능성 확보가 아니라 **중복 제거(DRY)** 이다.
- 기존 `runSampleMenu`/`runOrderMenu`/`runApprovalMenu`/`runReleaseMenu`를 `submenu::run()` 호출로
  교체하고, 신설하는 `runMonitoringMenu()`도 동일 헬퍼를 사용한다.

## 도메인 로직: `MonitoringService`

`prd.md` §4.5 기준. `SampleRepository`/`OrderRepository`를 읽기 전용으로 참조해 집계만 수행하고,
영속 상태를 갖지 않는다(별도 JSON 저장 없음 — 매번 그때그때 계산).

```cpp
// src/model/MonitoringService.h
struct OrderCountSummary {
    int reserved = 0;
    int confirmed = 0;
    int producing = 0;
    int release = 0;
    // REJECTED는 무효 주문이므로 집계에서 제외 (prd.md §4.5)
};

enum class InventoryLevel { PLENTY, LOW, DEPLETED };  // 여유/부족/고갈
std::string inventoryLevelToString(InventoryLevel level);

struct SampleInventoryStatus {
    Sample sample;
    int reservedQuantity;   // 해당 시료의 RESERVED 주문 quantity 합
    InventoryLevel level;
};

class MonitoringService {
public:
    MonitoringService(SampleRepository& sampleRepository, OrderRepository& orderRepository);

    OrderCountSummary orderCountSummary() const;
    std::vector<SampleInventoryStatus> inventoryStatus() const;  // SampleRepository::list() 순서 그대로

private:
    SampleRepository& sampleRepository_;
    OrderRepository& orderRepository_;
};
```

- `orderCountSummary()`: `orderRepository_.list()`를 순회하며 상태별로 세되 `REJECTED`는 건너뛴다.
- `inventoryStatus()`: `sampleRepository_.list()`의 각 시료에 대해 `orderRepository_.listByStatus(RESERVED)`
  중 `sampleId`가 일치하는 것들의 `quantity`를 합산한 뒤, 위 "확정된 결정 사항 1"의 3단계 판정을 적용한다.

## Controller / View

```cpp
// src/controller/MonitoringController.h
class MonitoringController {
public:
    MonitoringController(MonitoringService& service, IView& view);

    void showOrderSummary();      // orderCountSummary() -> view_.showOrderCountSummary
    void showInventoryStatus();   // inventoryStatus() -> view_.showInventoryStatus
};
```

```cpp
// src/view/IView.h — 추가
virtual void showOrderCountSummary(const OrderCountSummary& summary) = 0;
virtual void showInventoryStatus(const std::vector<SampleInventoryStatus>& statuses) = 0;
```

`MainController::handleSelection("4")`는 신설할 `runMonitoringMenu()`로 진입한다(`prd.md` §4.1). 하위 메뉴:
`[1] 주문량 확인 [2] 재고량 확인 [0] 뒤로가기`. `MainController` 생성자에 `MonitoringController&`가
추가된다 — Phase 5의 `ProductionController`와 마찬가지로 진짜 새 도메인이므로 정당한 증가다.

## 디렉토리/파일 변경 요약

| 파일 | 변경 |
|---|---|
| `src/controller/SubMenu.h/.cpp` | 신규. 서브메뉴 공통 루프 |
| `src/controller/MainController.h/.cpp` | 기존 4개 서브메뉴를 `submenu::run()` 호출로 리팩터링, "4" 선택 시 `runMonitoringMenu()` 신설, 생성자에 `MonitoringController&` 추가 |
| `src/model/MonitoringService.h/.cpp` | 신규 |
| `src/controller/MonitoringController.h/.cpp` | 신규 |
| `src/view/IView.h`, `ConsoleView.h/.cpp` | `showOrderCountSummary`/`showInventoryStatus` 추가 |
| `src/main.cpp` | `MonitoringService`/`MonitoringController` 생성·연결 |
| `tests/MonitoringServiceTest.cpp` | 신규 — REJECTED 제외 집계, 재고 판정 3구간 경계값(고갈/여유=경계 포함/부족), RESERVED만 비교 대상인지(PRODUCING/CONFIRMED/RELEASE 제외 확인) |
| `tests/MonitoringControllerTest.cpp` | 신규 — View 위임 확인 (`StubView` 재사용) |
| `tests/StubView.h` | `showOrderCountSummary`/`showInventoryStatus` 추가 |
| `tc.md` | 1부 인벤토리에 신규 테스트 반영 |

기존 서브메뉴 리팩토링에 대한 회귀 확인은 새 테스트를 추가하지 않고, Phase 1~6의 기존 `*ControllerTest`가
그대로 통과하는지로 검증한다 (Controller 로직 자체는 변경하지 않고 `MainController`의 루프 골격만 바뀌므로).

## Phase 7에서 하지 않는 것

- 메인 메뉴 요약 정보 통합 (Phase 8에서 `MonitoringService`를 재사용해 표시)
- 입력 파싱과 I/O 분리 (Phase 2~3에서부터 이어지는 별도 미검증 범위, 이번에도 다루지 않음)
- 시료별 상세 이력/차트 등 prd.md에 없는 부가 기능

## 완료 기준

- 상태별 주문 건수 집계 시 REJECTED 제외 테스트 (RESERVED/CONFIRMED/PRODUCING/RELEASE만 집계)
- 재고 판정 3구간 경계값 테스트: `stock==0`(고갈, RESERVED 유무 무관), `stock==RESERVED 합`(여유), `stock<RESERVED 합`(부족)
- PRODUCING/CONFIRMED/RELEASE 주문이 재고 판정 비교에 포함되지 않는지 확인하는 테스트
- 서브메뉴 헬퍼(`submenu::run`) 추출 후 기존 `SampleControllerTest`/`OrderControllerTest` 전체 회귀 없음
- `ctest` 전체(기존 60개 + 신규) 통과
- 콘솔 실행 로그로 "주문 여러 건(RESERVED/CONFIRMED 등 혼합) → 모니터링(주문량 확인, 재고량 확인)" 재현 (Demonstrability)
