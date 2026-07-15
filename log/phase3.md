# Phase 3 구현 완료 및 Verify 결과

- 관련 설계 문서: `docs/design/phase3.md`
- 관련 계획: `PLAN.md` Phase 3

## 구현 내용

| 파일 | 내용 |
|---|---|
| `src/model/Order.h` | 신규. `OrderStatus` enum, `Order` 구조체, `orderStatusToString`/`orderStatusFromString` |
| `src/model/OrderRepository.h/.cpp` | 신규. `Clock` 주입(기본값 `std::chrono::system_clock::now`), 주문번호(`ORD-YYYYMMDD-NNNN`) 생성, 예약/조회/목록. 날짜 계산은 Howard Hinnant의 civil_from_days 알고리즘으로 플랫폼 CRT 함수 없이 UTC 기준 결정론적 처리 |
| `src/controller/OrderInput.h/.cpp` | 신규. `std::cin`과 분리된 순수 함수 `parsePositiveQuantity` |
| `src/controller/OrderController.h/.cpp` | 신규. 주문 접수/목록 흐름 전담 |
| `src/controller/MainController.h/.cpp` | "2" 선택 시 `runOrderMenu()`로 진입, `orderinput::parsePositiveQuantity`로 수량 파싱 |
| `src/view/IView.h`, `ConsoleView.h/.cpp` | `showOrders`/`showOrder` 추가 |
| `src/main.cpp` | `OrderRepository`/`OrderController` 생성·연결 |
| `src/CMakeLists.txt` | `sample_order_core`에 `OrderRepository`/`OrderController`/`OrderInput` 추가 |
| `tests/OrderTestData.h` | 신규. `orderRequestSchema()`(quantity>0 반영), `toOrderRequest()`, 고정 `fixedClock()`(2026-04-16 09:00 UTC, civil_from_days 기반) |
| `tests/OrderInputTest.cpp` | 신규. 수량 파싱 성공/실패(0, 음수, 비숫자, 뒤 문자 붙음) |
| `tests/OrderRepositoryTest.cpp` | 신규. 주문번호 형식·순번 증가(고정 Clock), 미등록 시료 거부, dummygen 기반 수량 범위 위반 거부, 빈 고객명 거부, 영속화 |
| `tests/OrderControllerTest.cpp` | 신규. `StubView`로 View 위임 확인 (성공/미등록 시료 오류/목록 전달) |
| `tests/StubView.h` | `showOrders`/`showOrder`, `lastOrders`/`lastOrder` 추가 |
| `tests/CMakeLists.txt` | 신규 테스트 3개 추가 |

## Verify 결과

### 1. Test Verify — `scripts/build.sh`

```
$ bash scripts/build.sh
...
[100%] Built target sample_order_tests
...
Test project C:/Users/User/semi/build
 1/28 ... OrderControllerTest.ReserveOrderShowsSuccessMessage ................. Passed
 2/28 ... OrderControllerTest.ReserveOrderForUnknownSampleShowsError .......... Passed
 3/28 ... OrderControllerTest.ListOrdersForwardsToView ........................ Passed
 4/28 ... OrderRepositoryTest.ReserveGeneratesOrderNumberWithFixedDate ........ Passed
 5/28 ... OrderRepositoryTest.OrderNumberSequenceIncrementsForSameDate ........ Passed
 6/28 ... OrderRepositoryTest.RejectsUnregisteredSampleId ..................... Passed
 7/28 ... OrderRepositoryTest.RejectsNonPositiveQuantityFromDummyInvalidSet ... Passed
 8/28 ... OrderRepositoryTest.RejectsEmptyCustomerName ........................ Passed
 9/28 ... OrderRepositoryTest.PersistsAcrossReload ............................ Passed
10/28 ... OrderInputTest.ParsesPositiveInteger ................................ Passed
11/28 ... OrderInputTest.RejectsZero ........................................... Passed
12/28 ... OrderInputTest.RejectsNegative ....................................... Passed
13/28 ... OrderInputTest.RejectsNonNumeric ..................................... Passed
14/28 ... OrderInputTest.RejectsTrailingGarbage ................................ Passed
15~28/28 ... (Phase 1/2 기존 테스트) ........................................... Passed

100% tests passed, 0 tests failed out of 28
```

결과: **통과 (28/28, Phase 1/2의 기존 14개 포함 회귀 없음)**. 임의 완화/우회 없음.

### 2. Compliance Verify — `docs/design/phase3.md` / `PLAN.md` Phase 3 요구사항 대조

| 요구사항 | 충족 여부 |
|---|---|
| 등록된 시료 ID로만 주문 생성 가능 | O (`RejectsUnregisteredSampleId`) |
| 주문 생성 시 상태 RESERVED로 저장·조회 | O |
| 주문번호 `ORD-YYYYMMDD-NNNN` 형식, 같은 날짜 내 순번 1씩 증가 (고정 Clock 검증) | O |
| 수량 파싱 함수 성공/실패 케이스 단위 테스트 | O |
| 주문 등록 → 재시작 → 조회 시 RESERVED 유지 (영속화) | O |
| `ctest` 전체 통과 | O (28/28) |
| 콘솔 실행 로그로 시료 등록 → 주문 접수 → 목록 조회 재현 | O (아래 Demonstrability) |

### 3. Document Consistency Verify

구현 전 `CLAUDE.md`/`prd.md`/`PLAN.md`/`docs/design/phase3.md` 상호 참조 확인. Phase 2에서 확정된
`JsonStore.cache_` 전제와 입력 파싱 분리 방침을 그대로 따랐으며 추가 충돌 없음.

### 4. Demonstrability — 콘솔 실행 로그

```
$ ./build/src/sample_order_app.exe
...
선택 > 1
--- 시료 관리 ---
선택 > 1
시료 ID > S-001
이름 > 실리콘 웨이퍼
평균 생산시간(min/ea) > 0.5
수율(0~1) > 0.9
시료 등록 완료: S-001

선택 > 0
선택 > 2
--- 시료 주문 ---
선택 > 1
시료 ID > S-001
고객명 > 삼성전자 파운드리
주문 수량 > 200
주문 접수 완료: ORD-20260715-0001

선택 > 2
[ORD-20260715-0001] S-001 x 200 ea - 삼성전자 파운드리 (RESERVED)
```

미등록 시료로 주문 시도 시 거부 확인:
```
선택 > 2
--- 시료 주문 ---
선택 > 1
시료 ID > S-999
고객명 > 고객
주문 수량 > 10
Error: 등록되지 않은 시료 ID입니다: S-999
```

`data/data.json` 내용 (실행 후):
```json
{
  "samples": [
    { "id": "S-001", "name": "실리콘 웨이퍼", "avgProductionTimeMinutes": 0.5, "yield": 0.9, "stock": 0 }
  ],
  "orders": [
    { "orderNumber": "ORD-20260715-0001", "sampleId": "S-001", "customerName": "삼성전자 파운드리", "quantity": 200, "status": "RESERVED" }
  ]
}
```

주문번호 날짜(`20260715`)는 기본 `Clock`(`std::chrono::system_clock::now`)이 사용된 시스템 현재 날짜(2026-07-15)를 반영한 것으로, 테스트에서는 `testdata::fixedClock()`(2026-04-16 고정)을 주입해 결정론적으로 검증했다.

재현 커맨드:
```
bash scripts/build.sh
./build/src/sample_order_app.exe
```

## 검증하지 못한 내용

- 자정 근처(날짜 경계) 동시 주문 시 주문번호 순번 계산의 동시성/경쟁 조건은 검증하지 않음 (콘솔 단일 프로세스·단일 스레드 가정이므로 해당 없음).
- `MainController::runOrderMenu()`의 `std::cin` 기반 입력 루프 자체에 대한 자동화 테스트는 작성하지 않음 (콘솔 실행 로그로만 확인) — Phase 2와 동일한 미검증 범위.
