# Phase 5 설계: 생산 라인 (FIFO 큐)

`PLAN.md`의 Phase 5 목표(단일 생산 라인 FIFO 큐, 생산량/생산시간 계산, 생산완료 자동전환)에 대한 상세
설계. `PLAN.md`에 이미 확정되어 있는 "생산 규칙(필수 준수)" 6가지를 그대로 따르며, Phase 4 코드 리뷰에서
발견된 `store_.save()` 중복 호출 문제를 이번 Phase에서 해소한다.

## 확정된 결정 사항

### 1. 시각 비교 판정 방식 (PLAN.md 미결 사항 해소)

`Order`에 아래 필드를 추가해 생산 시작/완료 시각을 **epoch seconds(정수)**로 영속 저장한다. `time_point`를
직접 JSON에 넣지 않고 정수로 저장해 (역)직렬화를 단순화하고, `OrderRepository`가 이미 쓰고 있는
`std::chrono::duration_cast<std::chrono::seconds>(...).count()` 패턴(주문번호 날짜 계산)과 일관성을
유지한다.

```cpp
// src/model/Order.h — 추가 필드
int productionQuantity = 0;                              // 실 생산량 = ceil(shortageQuantity / yield), 생산 시작 시 계산·고정
std::optional<std::int64_t> productionStartedAtEpochSec;    // 생산 시작 시각. 대기 중(아직 미시작)이면 nullopt
std::optional<std::int64_t> productionCompletesAtEpochSec;  // 예정 완료 시각 = 시작 + 평균생산시간(분)*productionQuantity
```

판정 로직은 "지금 시각(`clock_()`)이 `productionCompletesAtEpochSec`을 지났는가"만 비교한다. 이 비교는
`ProductionQueue::advance()`가 **호출되는 시점마다** 수행되며, 프로세스가 그 사이 종료·재시작되었는지는
무관하다 (파일에 저장된 값만 근거로 판정 — PLAN.md "프로그램 생명주기 독립적 완료 판정" 규칙 그대로).

`advance()` 호출 시점: (1) `main()`에서 `store.ensureLoaded()` 직후 앱 시작 시 1회, (2) `MainController`가
"생산 라인 조회" 메뉴("5")로 진입할 때마다. 백그라운드 타이머/스레드는 두지 않는다(콘솔 앱, PLAN.md 범위
밖의 과설계 방지).

### 2. `store_.save()` 중복 호출 제거 (Phase 4 코드 리뷰 반영)

`SampleRepository`에 저장하지 않는 내부 변형 메서드를 추가하고, 복합 연산(승인/생산 큐 진행)은 이 메서드로
메모리만 바꾼 뒤 **자신이 마지막에 한 번만 `store_.save()`를 호출**한다.

```cpp
// src/model/SampleRepository.h — 추가
bool adjustStockInMemory(const std::string& id, int newStock);  // 저장하지 않음, cache_만 변경

// 기존 setStock은 이 메서드 + store_.save()로 재정의 (단독 호출용, 동작 동일)
bool setStock(const std::string& id, int newStock);
```

- `ProductionQueue::advance()`는 `adjustStockInMemory()`로 재고를 바꾸고, 주문 항목도 직접 갱신한 뒤
  마지막에 `store_.save()`를 한 번만 호출한다.
- **Phase 4에서 구현된 `OrderRepository::approve()`도 이번 Phase에서 함께 정리**한다 — `setStock()` 대신
  `adjustStockInMemory()`를 호출하도록 바꾸고 기존처럼 마지막에 한 번만 저장한다 (동작 동일, 저장 횟수만
  1회로 감소). 이 리팩토링은 Phase 5 범위로 명시적으로 포함한다 (Scope Creep 아님 — Phase 5 설계 문서에
  근거를 남겨둠).

### 3. 새 도메인 Controller 도입: `ProductionController`

Phase 4에서는 "승인/거절이 Order 도메인 자체의 상태 전이"라 기존 `OrderController`를 재사용했다. 이번
Phase의 "생산 현황/대기 목록 조회"는 Order/Sample과는 다른 새로운 관심사(생산 큐 진행)이므로, Phase 2에서
정한 원칙("도메인마다 필요하면 신설")에 따라 `ProductionController`를 새로 만든다. `MainController`
생성자에 `ProductionController&` 파라미터가 하나 늘어난다 — Phase 2에서 우려했던 "불필요한 확장"이
아니라 실제로 새로운 도메인이므로 정당한 증가로 판단.

## 도메인 로직: `ProductionQueue`

```cpp
// src/model/ProductionQueue.h
class ProductionQueue {
public:
    using Clock = std::function<std::chrono::system_clock::time_point()>;

    ProductionQueue(sampleorder::JsonStore& store, SampleRepository& sampleRepository,
                     OrderRepository& orderRepository, Clock clock = &std::chrono::system_clock::now);

    // 호출될 때마다 최대 한 단계만 진행한다:
    //   1) 활성 생산(productionStartedAt 있음)이 완료 시각을 지났으면: 재고에 productionQuantity를
    //      batch로 더하고 CONFIRMED 전환.
    //   2) 활성 생산이 없으면(막 완료했거나 원래 없었음): 대기 큐 맨 앞 주문의 생산을 시작
    //      (productionQuantity = ceil(shortageQuantity/yield) 계산·고정, productionStartedAt = now,
    //      productionCompletesAt = now + 평균생산시간(분)*productionQuantity).
    // 두 단계가 한 번의 advance() 호출 안에서 순차로 일어날 수 있다(완료 직후 다음 시작) — 이래도
    // "한 번에 하나만 활성 생산"이라는 규칙은 깨지지 않는다(완료 처리 후에는 활성 생산이 없는 상태이므로).
    void advance();

    std::vector<Order> waitingQueue() const;        // status==PRODUCING && !productionStartedAt, 배열 순서(FIFO)
    std::optional<Order> activeProduction() const;  // status==PRODUCING && productionStartedAt 있음

private:
    sampleorder::JsonStore& store_;
    SampleRepository& sampleRepository_;
    OrderRepository& orderRepository_;
    Clock clock_;
};
```

- **큐 병합 금지** 규칙: `waitingQueue()`가 배열에 있는 개별 주문을 그대로 반환하므로 병합이 발생할
  여지가 없다 (Phase 1에서 이미 결정한 "FIFO 순서는 배열 등장 순서" 방침을 그대로 따름).
- **PRODUCING 수량은 가용 재고 제외** 규칙: `Sample.stock` 필드 자체가 이미 승인 시점에 점유분만큼
  차감되어 있으므로(Phase 4), 별도 "가용재고 계산" 로직이 필요 없다 — `stock` 필드가 곧 가용 재고다.
- **수율 재적용 금지**: `productionQuantity`는 생산 "시작" 시점에 한 번만 계산해 `Order`에 고정 저장한다.
  완료 시에는 저장된 `productionQuantity`를 그대로 재고에 더할 뿐, 수율을 다시 곱하지 않는다.

## Controller / View

```cpp
// src/controller/ProductionController.h
class ProductionController {
public:
    ProductionController(ProductionQueue& queue, IView& view);

    void showStatus();  // advance() 호출 후 activeProduction()/waitingQueue()를 View에 전달
};
```

```cpp
// src/view/IView.h — 추가
virtual void showProductionStatus(const std::optional<Order>& active,
                                    const std::vector<Order>& waiting) = 0;
```

`MainController::handleSelection("5")`는 `productionController_.showStatus()`를 호출한다 (하위 메뉴 없이
바로 현황 출력 — prd.md §4.6에 별도 하위 메뉴 요구 없음, 조회 전용이므로 `datamon` PoC의 "읽기 전용
세션" 패턴과 유사).

`main.cpp`에서 `store.ensureLoaded()` 직후 `productionQueue.advance()`를 1회 호출해 재시작 시 밀린
생산완료를 즉시 반영한다.

## 디렉토리/파일 변경 요약

| 파일 | 변경 |
|---|---|
| `src/model/Order.h` | `productionQuantity`/`productionStartedAtEpochSec`/`productionCompletesAtEpochSec` 추가 |
| `src/model/OrderRepository.h/.cpp` | JSON (역)직렬화에 신규 필드 반영(`.value()` 기본값으로 Phase 4까지의 데이터 호환), `approve()`가 `adjustStockInMemory` 사용하도록 리팩터링(단일 저장) |
| `src/model/SampleRepository.h/.cpp` | `adjustStockInMemory` 추가, `setStock`은 이를 이용해 재정의 |
| `src/model/ProductionQueue.h/.cpp` | 신규 |
| `src/controller/ProductionController.h/.cpp` | 신규 |
| `src/controller/MainController.h/.cpp` | "5" 선택 시 `productionController_.showStatus()` 호출, 생성자에 `ProductionController&` 추가 |
| `src/view/IView.h`, `ConsoleView.h/.cpp` | `showProductionStatus` 추가 |
| `src/main.cpp` | `ProductionQueue`/`ProductionController` 생성·연결, 시작 시 `advance()` 1회 호출 |
| `tests/ProductionQueueTest.cpp` | 신규 — 생산 시작(수량/완료시각 계산), 완료 전 조회 시 변화 없음, 완료 후 CONFIRMED+재고 batch 반영, FIFO 순차 처리(병합 없음), 재시작 시나리오(고정 Clock으로 "이미 지난 완료시각" 주입 후 `advance()` 1회로 완료 확인) |
| `tests/OrderRepositoryTest.cpp` | 신규 필드 영속화 확인, `approve()` 리팩터링 후 회귀 없음 확인(저장 횟수 자체는 화이트박스로 검증하지 않고 동작 결과로만 확인) |

## Phase 5에서 하지 않는 것

- 출고 처리 (Phase 6)
- 모니터링 집계 (Phase 7) — 단, `waitingQueue().size()`는 Phase 8 메인 메뉴 요약 정보에서 재사용될 수 있음
- 여러 생산 라인 동시 운영 (prd.md 3.4 "단일 라인"에 반함, 범위 아님)

## 완료 기준

- 승인(재고부족)된 주문이 대기 큐에 FIFO 순서로 남아있는지 확인
- `advance()` 호출 시 활성 생산이 없으면 대기 큐 맨 앞 주문의 생산이 시작(수량/완료시각 계산)되는지 테스트
- 완료 시각 이전에는 상태가 그대로 PRODUCING인지, 완료 시각 이후에는 CONFIRMED + 재고 batch 반영(부분 반영 없음)되는지 테스트
- 여러 대기 주문이 병합되지 않고 개별 순차 처리되는지 테스트
- 고정 Clock으로 "이미 완료 시각이 지난 상태"를 주입해 재시작 시나리오(프로세스 생명주기 무관 판정)를 검증
- `Sample.stock`이 PRODUCING 동안 가용 재고에서 제외되어 있는지(이미 0으로 차감되어 있으므로 별도 로직 없이 자연히 만족) 확인
- `OrderRepository::approve()` 리팩터링 후 Phase 4 테스트 전체 회귀 없음
- `ctest` 전체(기존 41개 + 신규) 통과
- 콘솔 실행 로그로 "승인(재고부족) → 생산 라인 조회(대기) → (테스트에서는 고정 Clock로 시간 경과 시뮬레이션) → 생산 라인 조회(완료 확인)" 재현 (Demonstrability)
