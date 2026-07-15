# Phase 4 구현 완료 및 Verify 결과

- 관련 설계 문서: `docs/design/phase4.md`
- 관련 계획: `PLAN.md` Phase 4

## 구현 내용

| 파일 | 내용 |
|---|---|
| `src/model/Order.h` | `shortageQuantity` 필드 추가 (PRODUCING 전환 시에만 의미, 그 외 0) |
| `src/model/OrderRepository.h/.cpp` | `approve`/`reject`/`listByStatus` 추가. JSON (역)직렬화에 `shortageQuantity` 반영, `j.value("shortageQuantity", 0)`으로 Phase 3까지의 기존 데이터도 예외 없이 로드 |
| `src/model/SampleRepository.h/.cpp` | `setStock(id, newStock)` 추가 (절대값 설정, id 없으면 false) |
| `src/controller/OrderController.h/.cpp` | `approveOrder`/`rejectOrder`/`listReservedOrders` 추가 |
| `src/controller/MainController.h/.cpp` | "3" 선택 시 `runApprovalMenu()` 신설 (생성자 시그니처 변경 없음 — Controller 재사용 방침) |
| `tests/SampleRepositoryTest.cpp` | `setStock` 성공/존재하지 않는 id 케이스 추가 |
| `tests/OrderRepositoryTest.cpp` | 승인(충분/부족) 재고·상태·부족분, 거절, 이미 처리된 주문 재승인/재거절 거부, 미존재 주문번호 승인 거부, `listByStatus`, 승인 후 영속화 케이스 추가 |
| `tests/OrderControllerTest.cpp` | 승인/거절 성공·실패, RESERVED만 필터링되는 목록 조회 View 위임 케이스 추가 |

## Verify 결과

### 1. Test Verify — `scripts/build.sh`

```
$ bash scripts/build.sh
...
[100%] Built target sample_order_tests
...
Test project C:/Users/User/semi/build
 (OrderControllerTest 7개, OrderRepositoryTest 13개, OrderInputTest 5개,
  SampleControllerTest 5개, SampleRepositoryTest 7개, JsonStoreTest 3개, Placeholder 1개)

100% tests passed, 0 tests failed out of 41
```

결과: **통과 (41/41, Phase 1~3의 기존 28개 포함 회귀 없음)**. 임의 완화/우회 없음.

### 2. Compliance Verify — `docs/design/phase4.md` / `PLAN.md` Phase 4 요구사항 대조

| 요구사항 | 충족 여부 |
|---|---|
| 재고 충분 시 승인 → `Sample.stock` 정확히 차감, CONFIRMED 전환 | O (`ApproveWithSufficientStockDeductsAndConfirms`) |
| 재고 부족 시 승인 → `Sample.stock` 0으로 차감, `shortageQuantity` 정확 계산, PRODUCING 전환 | O (`ApproveWithInsufficientStockZeroesStockAndProduces`) |
| 거절 → REJECTED 전환, 재고 변경 없음 | O (`RejectTransitionsToRejectedWithoutStockChange`) |
| 이미 RESERVED가 아닌 주문의 재승인/재거절 거부 | O (`RejectsReapprovalOfNonReservedOrder`) |
| RESERVED 목록만 조회 (`listByStatus`) | O (`ListByStatusReturnsOnlyMatchingOrders`, Controller 쪽 `ListReservedOrdersForwardsOnlyReservedToView`) |
| 승인/거절 후 재시작 → 상태·재고 유지 | O (`ApprovalPersistsAcrossReload`) |
| `ctest` 전체 통과 | O (41/41) |
| 콘솔 실행 로그로 "접수 → RESERVED 목록 조회 → 승인(재고부족) → 재고 0 확인" 재현 | O (아래 Demonstrability) |

### 3. Document Consistency Verify

구현 전 `CLAUDE.md`/`prd.md`/`PLAN.md`/`docs/design/phase4.md` 상호 참조 확인. Phase 3 코드 리뷰에서
지적된 두 가지(Controller 재사용, `OrderRepository` 상태 변경 메서드 신설)를 설계·구현 모두에 반영했다.
추가 충돌 없음.

### 4. Demonstrability — 콘솔 실행 로그

```
$ ./build/src/sample_order_app.exe
...
선택 > 1        # 시료 관리 -> 등록 (재고 0으로 시작)
...
시료 등록 완료: S-001

선택 > 2        # 시료 주문 -> 접수 (200 ea)
...
주문 접수 완료: ORD-20260715-0001

선택 > 3        # 주문 승인/거절
--- 주문 승인/거절 ---
선택 > 1        # 접수된 주문 목록
[ORD-20260715-0001] S-001 x 200 ea - 삼성전자 파운드리 (RESERVED)

선택 > 2        # 주문 승인
주문번호 > ORD-20260715-0001
주문 승인 완료 (재고 부족, 생산 대기): ORD-20260715-0001

선택 > 1        # 접수된 주문 목록 재조회 -> 더 이상 RESERVED 아님
등록된 주문이 없습니다.
```

`data/data.json` 내용 (승인 후):
```json
{
  "samples": [
    { "id": "S-001", "name": "실리콘 웨이퍼", "avgProductionTimeMinutes": 0.5, "yield": 0.9, "stock": 0 }
  ],
  "orders": [
    {
      "orderNumber": "ORD-20260715-0001", "sampleId": "S-001", "customerName": "삼성전자 파운드리",
      "quantity": 200, "shortageQuantity": 200, "status": "PRODUCING"
    }
  ]
}
```

재고 0(등록 시 기본값) 대비 주문 200 → 부족분 200 전부가 `shortageQuantity`에 정확히 반영되고,
`stock`은 0으로 유지(추가 차감 없음)됨을 확인했다.

재현 커맨드:
```
bash scripts/build.sh
./build/src/sample_order_app.exe
```

## 검증하지 못한 내용

- 재고가 정확히 주문 수량과 같은 경계값(정확히 소진, 부족분 0)의 승인 케이스는 별도 테스트로 분리하지 않음 (충분 케이스 로직과 동일 분기이므로 낮은 리스크로 판단).
- `MainController::runApprovalMenu()`의 `std::cin` 기반 입력 루프 자체에 대한 자동화 테스트는 작성하지 않음 (콘솔 실행 로그로만 확인) — 이전 Phase와 동일한 미검증 범위.
