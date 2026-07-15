# Phase 6 구현 완료 및 Verify 결과

- 관련 설계 문서: `docs/design/phase6.md`
- 관련 계획: `PLAN.md` Phase 6

## 구현 내용

| 파일 | 내용 |
|---|---|
| `src/model/OrderRepository.h/.cpp` | `ReleaseResult`, `release(orderNumber)` 추가. `approve()`/`reject()`와 동일한 구조(주문 찾기 → 상태 가드 → 상태만 변경 → `store_.save()` 1회). 재고를 건드리지 않아 `SampleRepository` 호출이 없음 |
| `src/controller/OrderController.h/.cpp` | `releaseOrder`/`listConfirmedOrders` 추가 |
| `src/controller/MainController.h/.cpp` | "6" 선택 시 `runReleaseMenu()` 신설 (생성자 시그니처 변경 없음 — Controller 재사용 방침 계승) |
| `tests/OrderRepositoryTest.cpp` | 출고 성공(재고 불변), RESERVED/REJECTED/PRODUCING/이미 RELEASE 상태 거부, 존재하지 않는 주문번호 거부, 영속화 |
| `tests/OrderControllerTest.cpp` | 출고 성공/실패 View 위임, `listConfirmedOrders`가 CONFIRMED만 필터링(출고 완료 주문 제외)하는지 |
| `tc.md` | 1부 인벤토리에 신규 테스트 7개 반영 (2부 시나리오 매트릭스는 재고/타이밍과 무관하므로 대상 아님) |
| `PLAN.md` | Phase 6 구현 완료 노트 추가 |

## 사람 확인 사항 반영

- `.claude/settings.json`의 PostToolUse/PreToolUse 하네스(핵심 로직 변경 시 테스트 누락 알림, 직접
  cmake/ctest 실행 시 scripts/build.sh 권고, src 변경 후 log 누락 알림)가 이번 Phase 작업 중 실제로
  여러 차례 발동해 정상 동작을 확인했다.

## Verify 결과

### 1. Test Verify — `scripts/build.sh`

구현 중 `OrderRepositoryTest.RejectsReleaseOfNonConfirmedOrders`가 1회 실패했다. 원인은 구현 버그가
아니라 테스트 설계 실수였다: 같은 시료(`S-001`)에 대해 앞서 대량 주문(10000개)을 승인하면서 재고가
0으로 소진되었는데, 뒤이은 "정상적으로 CONFIRMED 후 출고" 케이스에서 재고를 다시 채우지 않아 해당
주문도 재고 부족으로 PRODUCING이 되어버렸다. 테스트에 `sampleRepo.setStock("S-001", 500)` 재설정을
추가해 수정했다.

```
$ bash scripts/build.sh
...
[100%] Built target sample_order_tests
...
Test project C:/Users/User/semi/build
 (60개 테스트 전체)

100% tests passed, 0 tests failed out of 60
```

결과: **통과 (60/60, Phase 1~5의 기존 53개 포함 회귀 없음)**. 임의 완화/우회 없음.

### 2. Compliance Verify — `docs/design/phase6.md` / `PLAN.md` Phase 6 요구사항 대조

| 요구사항 | 충족 여부 |
|---|---|
| CONFIRMED 상태 주문만 출고 가능 | O (`RejectsReleaseOfNonConfirmedOrders`) |
| 출고 시 CONFIRMED → RELEASE 전환 | O |
| 출고 시 재고 변경 없음 (승인 시점에 이미 반영) | O (`ReleaseTransitionsConfirmedToReleaseWithoutStockChange`) |
| 존재하지 않는 주문번호 출고 거부 | O (`ReleaseUnknownOrderNumberFails`) |
| 출고 대상(CONFIRMED) 목록만 조회 | O (`ListConfirmedOrdersForwardsOnlyConfirmedToView`) |
| 출고 후 재시작 → RELEASE 상태 유지 | O (`ReleasePersistsAcrossReload`) |
| `ctest` 전체 통과 | O (60/60) |
| 콘솔 실행 로그로 "승인(재고충분)→출고 대상 목록 조회→출고 처리→목록에서 사라짐" 재현 | O (아래 Demonstrability) |

### 3. Document Consistency Verify

구현 전 `CLAUDE.md`/`prd.md`/`PLAN.md`/`docs/design/phase6.md` 상호 참조 확인. Phase 4에서 확정된
"출고는 재고를 건드리지 않는다" 결정과 Controller 재사용 원칙을 그대로 따랐으며 추가 충돌 없음.

### 4. Demonstrability — 콘솔 실행 로그

시료를 재고 0으로 등록한 뒤, 생산을 완료시켜 재고를 확보하고, 두 번째 주문을 재고로 즉시 충당(CONFIRMED)한 뒤 출고까지 진행했다.

```
# 1차 실행: 등록 -> 주문1(5ea) 접수 -> 승인(재고부족, PRODUCING) -> [5] 생산 라인 조회로 생산 시작
현재 생산 중: [ORD-20260715-0001] S-001, 생산량 5 ea

# (실제 5초 대기)

# 2차 실행(재시작 시 advance() 1회로 생산 완료 자동 반영, 재고 5)
선택 > 2   # 주문2(3ea) 접수
주문 접수 완료: ORD-20260715-0002
선택 > 3   # 승인 -> 재고 5 >= 3 이므로 즉시 CONFIRMED
주문 승인 완료: ORD-20260715-0002
선택 > 6   # 출고 처리 -> 대상 목록
[ORD-20260715-0001] S-001 x 5 ea - 고객 (CONFIRMED)
[ORD-20260715-0002] S-001 x 3 ea - 고객2 (CONFIRMED)

# 3차 실행: 주문2 출고 처리
선택 > 6
선택 > 2
주문번호 > ORD-20260715-0002
출고 처리 완료: ORD-20260715-0002
선택 > 1   # 출고 대상 목록 재조회 -> 주문2는 더 이상 CONFIRMED 아님
[ORD-20260715-0001] S-001 x 5 ea - 고객 (CONFIRMED)
```

`data/data.json` 내용 (출고 후):
```json
{
  "samples": [
    { "id": "S-001", "name": "테스트", "avgProductionTimeMinutes": 0.0166, "yield": 1.0, "stock": 2 }
  ],
  "orders": [
    { "orderNumber": "ORD-20260715-0001", "sampleId": "S-001", "customerName": "고객",
      "quantity": 5, "shortageQuantity": 5, "productionQuantity": 5, "status": "CONFIRMED",
      "productionStartedAtEpochSec": 1784088899, "productionCompletesAtEpochSec": 1784088903 },
    { "orderNumber": "ORD-20260715-0002", "sampleId": "S-001", "customerName": "고객2",
      "quantity": 3, "shortageQuantity": 0, "productionQuantity": 0, "status": "RELEASE",
      "productionStartedAtEpochSec": null, "productionCompletesAtEpochSec": null }
  ]
}
```

재고(2 = 5 - 3)는 출고 처리로 변경되지 않고 승인 시점 그대로이며, 주문1(CONFIRMED)은 출고하지 않아 그대로 남아있음을 확인했다.

재현 커맨드:
```
bash scripts/build.sh
./build/src/sample_order_app.exe
```

## 검증하지 못한 내용

- `MainController::runReleaseMenu()`의 `std::cin` 기반 입력 루프 자체에 대한 자동화 테스트는 작성하지 않음 (콘솔 실행 로그로만 확인) — 이전 Phase와 동일한 미검증 범위.
