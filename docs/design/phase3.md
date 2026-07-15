# Phase 3 설계: 주문(Order) 모델 및 예약 접수

`PLAN.md`의 Phase 3 목표(주문 예약(RESERVED) 등록 기능 완성)에 대한 상세 설계. Phase 2 코드 리뷰에서
발견되어 `PLAN.md`에 반영된 2가지 확정 필요 사항(`JsonStore.cache_` 전제, 입력 파싱 분리)을 이번
Phase에서 반영한다.

## 확정된 결정 사항

### 1. 주문번호 생성: `ORD-YYYYMMDD-NNNN` (사람 확인 완료)

`prd.md` §3.2 예시(`ORD-20260416-0043`) 형식을 그대로 채택한다.

- `YYYYMMDD`: 주문 생성 시각의 날짜
- `NNNN`: 해당 날짜(prefix가 동일한) 기존 주문 건수 + 1, 4자리 0-패딩 (같은 날 5번째 주문이면 `0005`)
- 날짜 소스는 하드코딩된 `std::chrono::system_clock::now()` 호출이 아니라, `json_crud` PoC의
  `Backup`(`std::function<std::chrono::system_clock::time_point()>` 시계 주입 패턴)을 그대로 차용해
  `OrderRepository` 생성자에서 주입 가능하게 한다. 테스트에서 고정된 시각을 주입해 날짜 의존적인
  주문번호 생성을 결정론적으로 검증할 수 있다.

```cpp
// src/model/OrderRepository.h
class OrderRepository {
public:
    using Clock = std::function<std::chrono::system_clock::time_point()>;

    OrderRepository(sampleorder::JsonStore& store, SampleRepository& sampleRepository,
                     Clock clock = &std::chrono::system_clock::now);
    ...
private:
    std::string generateOrderNumber() const;  // YYYYMMDD 계산 + store_.orders() 내 동일 prefix 개수 세기
    Clock clock_;
};
```

### 2. `JsonStore.cache_` 전제로 설계 (Phase 2 코드 리뷰 반영)

`docs/design/phase2.md` §4에서 확인된 대로, `JsonStore`는 내부 `cache_`가 실제 데이터를 들고 있고
`save()` 시점에 `doc_`로 복사된다. `OrderRepository`도 `SampleRepository`와 동일하게 `store_.orders()`를
직접 읽고 수정한 뒤 `store_.save()`를 호출하는 방식으로 설계한다 (별도 캐시를 두지 않음).

### 3. 입력 파싱과 I/O 분리 적용 (Phase 2 코드 리뷰 반영)

주문 접수는 시료ID/고객명/수량 3개 입력을 받아야 해 Phase 2의 시료 등록보다 입력이 많다. 이번 Phase부터
**파싱/검증 로직을 `std::cin`과 분리된 순수 함수로 추출**한다.

```cpp
// src/controller/OrderInput.h (신규, 순수 함수 — 테스트 가능)
namespace orderinput {
// 성공 시 true, count에 파싱된 값 저장. 실패(숫자 아님, 0 이하) 시 false.
bool parsePositiveQuantity(const std::string& raw, int& outQuantity);
}
```

`MainController::runOrderMenu()`는 `std::cin`으로 문자열만 읽고, 수량 파싱은 `orderinput::parsePositiveQuantity`를
호출한다. 이 함수는 `std::cin` 의존이 없으므로 `OrderInputTest.cpp`에서 문자열 입력만으로 단위 테스트할 수 있다.
(Phase 2의 `MainController::runSampleMenu()`는 이번 Phase 범위에서 리팩토링하지 않는다 — Phase 3 범위를
벗어난 Scope Creep 방지. 필요하면 별도로 요청.)

## 도메인 모델

`prd.md` §3.2~§3.3, §4.3 기준.

```cpp
// src/model/Order.h
enum class OrderStatus { RESERVED, REJECTED, PRODUCING, CONFIRMED, RELEASE };

struct Order {
    std::string orderNumber;     // 자동 생성 (사용자 입력 아님)
    std::string sampleId;        // 등록된 Sample이어야 함 (SampleRepository로 존재 검증)
    std::string customerName;
    int quantity;                // > 0
    OrderStatus status = OrderStatus::RESERVED;
};
```

```cpp
// src/model/OrderRepository.h
class OrderRepository {
public:
    using Clock = std::function<std::chrono::system_clock::time_point()>;
    OrderRepository(sampleorder::JsonStore& store, SampleRepository& sampleRepository,
                     Clock clock = &std::chrono::system_clock::now);

    // 실패 사유: 존재하지 않는 sampleId, quantity <= 0, customerName 빈 문자열
    struct ReserveResult { bool success; std::string errorMessage; std::optional<Order> order; };
    ReserveResult reserve(const std::string& sampleId, const std::string& customerName, int quantity);

    std::optional<Order> find(const std::string& orderNumber) const;
    std::vector<Order> list() const;

private:
    sampleorder::JsonStore& store_;
    SampleRepository& sampleRepository_;   // 주문 대상 시료 존재 검증에 사용 (Phase 2 산출물 재사용)
    Clock clock_;
    std::string generateOrderNumber() const;
};
```

- `SampleRepository&`를 주입받아 `find(sampleId)`로 존재 여부를 검증한다 — `prd.md` §4.2 "시스템에 등록된
  시료만 주문 가능" 규칙을 Phase 3에서 그대로 적용.
- `Order` ↔ JSON 변환은 `SampleRepository.cpp`의 `toJson`/`fromJson` 패턴을 그대로 따른다. `OrderStatus`는
  문자열("RESERVED" 등)로 직렬화한다.

## View / Controller

```cpp
// src/view/IView.h — Order 표시 메서드 추가
virtual void showOrders(const std::vector<Order>& orders) = 0;
virtual void showOrder(const Order& order) = 0;
```

```cpp
// src/controller/OrderController.h (신규 — Phase 2에서 확정한 "도메인별 Controller" 방향 계승)
class OrderController {
public:
    OrderController(OrderRepository& repository, IView& view);

    void reserveOrder(const std::string& sampleId, const std::string& customerName, int quantity);
    void listOrders();
};
```

`MainController::handleSelection("2")`는 신설할 `runOrderMenu()`로 진입한다 (`prd.md` §4.1 메인 메뉴의
"주문(접수)" 항목). 하위 메뉴: `[1] 주문 접수 [2] 주문 목록 [0] 뒤로가기`. `runSampleMenu()`와 동일한
구조를 따르되, 수량 입력만 `orderinput::parsePositiveQuantity`를 사용한다.

## 디렉토리/파일 변경 요약

| 파일 | 변경 |
|---|---|
| `src/model/Order.h` | 신규 |
| `src/model/OrderRepository.h/.cpp` | 신규 |
| `src/controller/OrderInput.h/.cpp` | 신규 (파싱 전용 순수 함수) |
| `src/controller/OrderController.h/.cpp` | 신규 |
| `src/controller/MainController.h/.cpp` | "2" 선택 시 `runOrderMenu()`로 진입하도록 수정 |
| `src/view/IView.h`, `ConsoleView.h/.cpp` | `showOrders`/`showOrder` 추가 |
| `src/main.cpp` | `OrderRepository`/`OrderController` 생성·연결 추가 |
| `tests/OrderInputTest.cpp` | 신규 — 수량 파싱 성공/실패 케이스 |
| `tests/OrderRepositoryTest.cpp` | 신규 — 예약 성공/미등록 시료 거부/수량 검증/주문번호 형식·순번 증가(고정 Clock 주입)/영속화 |
| `tests/OrderControllerTest.cpp` | 신규 — `StubView`로 View 위임 확인 (Phase 2 `StubView.h` 재사용) |
| `tests/dummygen/` | 재사용 — Order 스키마용 `orderSchema()`를 `tests/OrderTestData.h`(신규)에 추가, 기존 `DummyGenerator`/`SchemaValidator`는 그대로 |

## Phase 3에서 하지 않는 것

- 주문 승인/거절 로직, 재고 확인 분기 (Phase 4)
- 생산 큐 연동 (Phase 5)
- 주문 검색/필터 UI (prd.md에 Phase 3 범위로 명시되어 있지 않음 — 모니터링 집계는 Phase 7)
- `MainController::runSampleMenu()`의 파싱 리팩토링 (Scope Creep 방지, 필요 시 별도 요청)

## 완료 기준

- 등록된 시료 ID로만 주문 생성 가능 (미등록 시료 ID 주문 시도 시 거부 테스트)
- 주문 생성 시 상태가 RESERVED로 저장·조회됨
- 주문번호가 `ORD-YYYYMMDD-NNNN` 형식으로 생성되고, 같은 날짜 내 순번이 1씩 증가하는지 고정 Clock 주입으로 검증
- 수량 파싱 함수(`orderinput::parsePositiveQuantity`)의 성공/실패 케이스 단위 테스트
- 주문 등록 → 앱 재시작 → 조회 시 동일 주문이 RESERVED 상태로 남아있음 (영속화 확인, `data/data.json` 재현)
- `ctest` 전체(기존 14개 + 신규) 통과
- 콘솔 실행 로그로 시료 등록 → 주문 접수 → 주문 목록 조회 흐름 재현 (Demonstrability)
