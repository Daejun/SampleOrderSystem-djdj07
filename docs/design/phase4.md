# Phase 4 설계: 주문 승인/거절 로직

`PLAN.md`의 Phase 4 목표(재고 확인 기반 자동 분기 및 거절 처리)에 대한 상세 설계. Phase 3 코드 리뷰에서
발견되어 `PLAN.md`에 반영된 2가지 확정 필요 사항(Controller 재사용, `OrderRepository` 상태 변경 메서드)을
이번 Phase에서 해소한다.

## 확정된 결정 사항 (사람 확인 완료)

### 1. 재고 차감 시점: 승인(CONFIRMED) 즉시

재고가 충분해 즉시 CONFIRMED로 전환되는 경우, **승인 시점에 주문 수량만큼 `Sample.stock`을 바로 차감**한다.
이렇게 해야 이후 다른 주문을 승인할 때 이미 점유된 재고를 중복 점유하지 않는다. 출고(Phase 6)는 상태만
`CONFIRMED → RELEASE`로 바꿀 뿐 재고를 다시 건드리지 않는다 (재고는 승인 시점에 이미 반영 완료).

### 2. 재고 부족 시: 기존 재고 전량을 해당 주문에 즉시 점유

재고가 부족해 PRODUCING으로 전환되는 경우(예: 재고 50, 주문 200), **승인 즉시 기존 재고 50도 0으로
차감**한다 — 이 50은 암묵적으로 해당 주문에 전액 점유된 것으로 취급한다. 부족분(`150 = 200 - 50`)은
`Order.shortageQuantity`에 저장해 Phase 5가 실 생산량(`ceil(150 / 수율)`) 계산에 그대로 사용한다.
생산이 완료되면(Phase 5) 실 생산량 전량이 재고에 더해지고 주문이 CONFIRMED로 전환된다 — 이 때 재고가
다시 차감되지는 않는다(이미 승인 시점에 이 주문 몫만큼은 재고 0으로 반영되어 있었으므로, 생산량이
부족분보다 많아 남는 초과분만 일반 재고 풀에 남는다).

### 3. Controller 재사용 — 새 도메인 Controller 신설하지 않음 (Phase 3 코드 리뷰 반영)

`MainController`가 도메인이 늘어날 때마다 생성자 파라미터를 계속 추가받는 문제를 막기 위해, 승인/거절은
**기존 `OrderController`에 메서드를 추가**하는 방식으로 처리한다 (Order 도메인 자체의 상태 전이이므로
별도 Controller를 만들 이유가 없다). `MainController`의 생성자 시그니처는 Phase 3과 동일하게 유지된다.

### 4. `OrderRepository` 상태 변경 메서드 신설 (Phase 3 코드 리뷰 반영)

```cpp
// src/model/OrderRepository.h — 기존 reserve/find/list에 추가
struct ApproveResult { bool success; std::string errorMessage; std::optional<Order> order; };
ApproveResult approve(const std::string& orderNumber);

struct RejectResult { bool success; std::string errorMessage; };
RejectResult reject(const std::string& orderNumber);

std::vector<Order> listByStatus(OrderStatus status) const;  // RESERVED 목록 조회 등에 재사용
```

- 실패 사유: 존재하지 않는 주문번호, 이미 RESERVED가 아닌 주문(중복 승인/거절 방지 — prd.md에 명시되어
  있지 않아 합리적으로 추가한 가드. 다른 정책을 원하면 알려달라).
- `approve()` 내부에서 `SampleRepository`의 재고를 함께 갱신해야 하므로, `SampleRepository&`(Phase 3에서
  이미 주입받고 있음)를 통해 아래 신설 메서드를 호출한다.

## `SampleRepository` 확장: 재고 직접 설정 메서드 신설

```cpp
// src/model/SampleRepository.h — 기존 registerSample/find/list/search에 추가
bool setStock(const std::string& id, int newStock);  // id 없으면 false
```

- 절대값 설정 방식(delta 가감이 아님)으로 설계 — 호출부(`OrderRepository::approve`)가 "충분 시
  `stock - quantity`", "부족 시 `0`"을 직접 계산해 전달하므로 단순하다.

## 도메인 모델 변경

```cpp
// src/model/Order.h
struct Order {
    std::string orderNumber;
    std::string sampleId;
    std::string customerName;
    int quantity = 0;
    int shortageQuantity = 0;   // 신규. PRODUCING 전환 시에만 의미 있음, 그 외 0
    OrderStatus status = OrderStatus::RESERVED;
};
```

- JSON 직렬화 시 `shortageQuantity`를 추가하되, 역직렬화는 `j.value("shortageQuantity", 0)`으로 읽어
  Phase 3까지 생성된 기존 `data.json`(이 필드가 없는 데이터)도 예외 없이 로드되도록 한다.

## `OrderRepository::approve` 로직

```
1. order = find(orderNumber); 없으면 실패("존재하지 않는 주문번호")
2. order.status != RESERVED 이면 실패("이미 처리된 주문입니다")
3. sample = sampleRepository_.find(order.sampleId)  // Phase 3에서 이미 존재 검증된 sampleId이므로 항상 존재
4. if (sample.stock >= order.quantity):
     sampleRepository_.setStock(sample.id, sample.stock - order.quantity)
     order.status = CONFIRMED; order.shortageQuantity = 0
   else:
     order.shortageQuantity = order.quantity - sample.stock
     sampleRepository_.setStock(sample.id, 0)
     order.status = PRODUCING
5. store_.orders() 내 해당 항목을 갱신 후 store_.save()
6. 성공 반환 (갱신된 order 포함)
```

`reject()`는 `order.status != RESERVED`면 실패, 아니면 `REJECTED`로 전환 후 저장 (재고 변경 없음).

## Controller / View

```cpp
// src/controller/OrderController.h — 기존 reserveOrder/listOrders에 추가
void approveOrder(const std::string& orderNumber);
void rejectOrder(const std::string& orderNumber);
void listReservedOrders();  // listByStatus(RESERVED) 결과를 showOrders로 전달
```

`IView`는 변경하지 않는다 (`showOrders`/`showOrder`를 그대로 재사용).

`MainController::handleSelection("3")`은 신설할 `runApprovalMenu()`로 진입한다 (`prd.md` §4.4). 하위 메뉴:
`[1] 접수된 주문 목록(RESERVED) [2] 주문 승인 [3] 주문 거절 [0] 뒤로가기`. 승인/거절은 주문번호를 입력받아
`OrderController::approveOrder`/`rejectOrder`를 호출한다 (기존 `runOrderMenu()`와 동일한 구조).

## 디렉토리/파일 변경 요약

| 파일 | 변경 |
|---|---|
| `src/model/Order.h` | `shortageQuantity` 필드 추가 |
| `src/model/OrderRepository.h/.cpp` | `approve`/`reject`/`listByStatus` 추가, JSON (역)직렬화에 `shortageQuantity` 반영 |
| `src/model/SampleRepository.h/.cpp` | `setStock` 추가 |
| `src/controller/OrderController.h/.cpp` | `approveOrder`/`rejectOrder`/`listReservedOrders` 추가 |
| `src/controller/MainController.h/.cpp` | "3" 선택 시 `runApprovalMenu()` 신설 (생성자 변경 없음) |
| `tests/OrderRepositoryTest.cpp` | 승인(충분/부족) 재고·상태·부족분 계산, 거절, 이미 처리된 주문 재승인/재거절 거부, 영속화 |
| `tests/SampleRepositoryTest.cpp` | `setStock` 성공/존재하지 않는 id 테스트 |
| `tests/OrderControllerTest.cpp` | 승인/거절 성공·실패 시 View 위임 확인 (`StubView` 재사용) |

## Phase 4에서 하지 않는 것

- 생산 큐 자료구조·타이머·생산 완료 자동 전환 (Phase 5) — `shortageQuantity`를 저장해두는 것까지만 이번
  Phase 범위
- 출고 처리 (Phase 6)
- 모니터링 집계 (Phase 7)

## 완료 기준

- 재고 충분 시 승인 → `Sample.stock` 정확히 차감, 주문 CONFIRMED 전환 테스트
- 재고 부족 시 승인 → `Sample.stock` 0으로 차감, `shortageQuantity` 정확 계산, 주문 PRODUCING 전환 테스트
- 거절 → REJECTED 전환, 재고 변경 없음 테스트
- 이미 RESERVED가 아닌 주문의 재승인/재거절 시도 시 거부 테스트
- RESERVED 목록만 조회되는지 테스트 (`listByStatus`)
- 승인/거절 후 재시작 → 상태·재고가 그대로 유지되는지 확인 (영속화)
- `ctest` 전체(기존 28개 + 신규) 통과
- 콘솔 실행 로그로 "주문 접수 → RESERVED 목록 조회 → 승인(재고부족 케이스) → 재고 0 확인"까지 재현 (Demonstrability)
