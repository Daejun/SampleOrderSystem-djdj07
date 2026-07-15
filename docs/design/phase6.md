# Phase 6 설계: 출고 처리

`PLAN.md`의 Phase 6 목표(CONFIRMED 상태 주문의 출고 처리)에 대한 상세 설계. 이번 Phase는 Phase 4/5에서
이미 확정된 재고 정책 위에서 **상태 전이 하나만 추가**하는 비교적 단순한 범위다.

## 확정된 결정 사항

### 1. 재고는 변경하지 않음 (Phase 4 결정의 연장)

`docs/design/phase4.md`에서 이미 "출고(Phase 6)는 상태만 `CONFIRMED → RELEASE`로 바꿀 뿐 재고를 다시
건드리지 않는다 — 재고는 승인 시점에 이미 반영 완료"라고 확정해 두었다. 이번 Phase는 그 결정을 그대로
따른다 — 별도 확인 질문 없이 상태 전이만 구현한다.

### 2. Controller 재사용 (Phase 4/5 원칙 계승)

출고도 승인/거절과 마찬가지로 **Order 도메인 자체의 상태 전이**이므로 새 Controller를 만들지 않고
기존 `OrderController`에 메서드를 추가한다. `MainController` 생성자 시그니처는 Phase 5와 동일하게
유지된다 (Phase 5에서 `ProductionController`가 늘어난 것과 달리, 이번엔 새 도메인이 아니므로 증가 없음).

## `OrderRepository` 확장

```cpp
// src/model/OrderRepository.h — 기존 reserve/approve/reject/find/list/listByStatus에 추가
struct ReleaseResult { bool success; std::string errorMessage; std::optional<Order> order; };

// 실패 사유: 존재하지 않는 주문번호, CONFIRMED가 아닌 주문(RESERVED/PRODUCING/REJECTED/이미 RELEASE).
// 재고는 변경하지 않는다.
ReleaseResult release(const std::string& orderNumber);
```

구현은 `approve()`/`reject()`와 동일한 구조(주문 찾기 → 상태 가드 → 상태만 변경 → `orderToJson` 갱신 →
`store_.save()` 1회)를 따른다. 재고를 건드리지 않으므로 `SampleRepository` 호출이 없다 — Phase 4/5에서
정리한 "이중 저장" 문제가 애초에 발생할 여지가 없다.

## Controller / View

```cpp
// src/controller/OrderController.h — 추가
void releaseOrder(const std::string& orderNumber);
void listConfirmedOrders();  // listByStatus(CONFIRMED) 결과를 showOrders로 전달 (listReservedOrders와 동일 패턴)
```

`IView`는 변경하지 않는다 (`showOrders`/`showOrder` 재사용).

`MainController::handleSelection("6")`은 신설할 `runReleaseMenu()`로 진입한다. 하위 메뉴:
`[1] 출고 대상 주문 목록(CONFIRMED) [2] 출고 처리 [0] 뒤로가기` — `runApprovalMenu()`와 동일한 구조.

## 디렉토리/파일 변경 요약

| 파일 | 변경 |
|---|---|
| `src/model/OrderRepository.h/.cpp` | `release()` 추가 |
| `src/controller/OrderController.h/.cpp` | `releaseOrder`/`listConfirmedOrders` 추가 |
| `src/controller/MainController.h/.cpp` | "6" 선택 시 `runReleaseMenu()` 신설 (생성자 변경 없음) |
| `tests/OrderRepositoryTest.cpp` | 출고 성공(CONFIRMED→RELEASE, 재고 불변), RESERVED/PRODUCING/REJECTED/이미 RELEASE 상태에서 거부, 존재하지 않는 주문번호 거부, 영속화 |
| `tests/OrderControllerTest.cpp` | 출고 성공/실패 View 위임, `listConfirmedOrders`가 CONFIRMED만 필터링하는지 |
| `tc.md` | 1부 인벤토리에 신규 테스트 추가 (2부 시나리오 매트릭스는 재고/타이밍과 무관하므로 대상 아님) |

## Phase 6에서 하지 않는 것

- 모니터링 집계 (Phase 7)
- 메인 메뉴 요약 정보 통합 (Phase 8)
- 출고 취소/반품 등 prd.md에 없는 기능

## 완료 기준

- CONFIRMED 주문 출고 → RELEASE 전환, 재고 변경 없음 테스트
- RESERVED/PRODUCING/REJECTED/이미 RELEASE 상태의 주문 출고 시도 시 거부 테스트
- 존재하지 않는 주문번호 출고 시도 시 거부 테스트
- 출고 대상(CONFIRMED) 목록만 조회되는지 테스트 (`listConfirmedOrders`)
- 출고 후 재시작 → RELEASE 상태 유지 확인 (영속화)
- `ctest` 전체(기존 53개 + 신규) 통과
- 콘솔 실행 로그로 "승인(재고충분) → 출고 대상 목록 조회 → 출고 처리 → 목록에서 사라짐" 재현 (Demonstrability)
