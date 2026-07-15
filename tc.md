# TC: 테스트 케이스 카탈로그

이 문서는 이 프로젝트에 실제로 구현되어 있는 모든 gtest 테스트 케이스의 목록(1부)과, 그중 승인
(`OrderRepository::approve`)·생산 큐(`ProductionQueue::advance`)의 동작을 "초기 재고량"과 "주문
도착/승인 타이밍"의 조합으로 일반화한 시나리오 상세(2부)로 구성된다. 2부는 `PLAN.md`의 Phase 4·5
생산 규칙과 `prd.md` §4.5(주문 승인/거절), §4.6(생산라인)을 근거로 하며, 실제 코드를 그대로 실행해
값을 검증했다.

## 1부. 전체 테스트 케이스 인벤토리 (Phase 1~7, 68개)

`bash scripts/build.sh` 기준 최신 실행 결과: **68/68 통과**. 파일별로 정리했다.

### `PlaceholderTest.cpp` — Phase 1 (빌드 확인용)

| 테스트 | 검증 내용 |
|---|---|
| `Placeholder.BuildWorks` | gtest 연동 및 빌드 체계가 정상 동작하는지 확인 |

### `JsonStoreTest.cpp` — Phase 2 (영속성 계층)

| 테스트 | 검증 내용 |
|---|---|
| `JsonStoreTest.BootstrapsWhenFileMissing` | `data.json` 파일이 없으면 `{"samples":[],"orders":[]}`로 자동 생성 |
| `JsonStoreTest.LoadsExistingFile` | 기존 파일이 있으면 그대로 로드 |
| `JsonStoreTest.SaveThenReloadRoundTrips` | 저장 후 재로드 시 데이터가 그대로 유지(라운드트립) |

### `SampleRepositoryTest.cpp` — Phase 2/5 (시료 CRUD, 재고 조작)

| 테스트 | 검증 내용 |
|---|---|
| `RegisterAndFindDummySample` | dummygen으로 생성한 유효 시료 등록 후 조회 일치 (등록 시 재고 0 강제 확인 포함) |
| `RejectsDuplicateId` | 동일 ID 중복 등록 거부 |
| `RejectsRangeViolationsFromDummyInvalidSet` | dummygen invalid 세트 중 생산시간≤0/수율 범위 밖 케이스 거부 |
| `SearchMatchesPartialCaseInsensitive` | id/name 등 부분 일치·대소문자 무시 검색 |
| `SetStockUpdatesExistingSample` | `setStock`으로 재고 절대값 설정 |
| `SetStockReturnsFalseForUnknownId` | 존재하지 않는 id에 대한 `setStock`은 false |
| `PersistsAcrossReload` | 등록 후 재시작해도 조회 가능(영속화) |

### `SampleControllerTest.cpp` — Phase 2 (Controller ↔ View 위임)

| 테스트 | 검증 내용 |
|---|---|
| `RegisterSampleShowsSuccessMessage` | 등록 성공 시 성공 메시지를 View에 전달 |
| `RegisterDuplicateShowsError` | 중복 등록 시 오류 메시지 전달 |
| `RegisterInvalidYieldShowsError` | 잘못된 수율 입력 시 오류 메시지 전달 |
| `ListSamplesForwardsToView` | 목록 조회 결과를 View에 그대로 전달 |
| `SearchSamplesForwardsToView` | 검색 결과를 View에 그대로 전달 |

### `OrderInputTest.cpp` — Phase 3 (입력 파싱 순수 함수)

| 테스트 | 검증 내용 |
|---|---|
| `ParsesPositiveInteger` | 양의 정수 문자열 파싱 성공 |
| `RejectsZero` | "0" 입력 거부 |
| `RejectsNegative` | 음수 입력 거부 |
| `RejectsNonNumeric` | 숫자가 아닌 입력 거부 |
| `RejectsTrailingGarbage` | 뒤에 불필요한 문자가 붙은 입력 거부 |

### `OrderRepositoryTest.cpp` — Phase 3/4/5/6 (주문 예약·승인·거절·출고)

| 테스트 | 검증 내용 |
|---|---|
| `ReserveGeneratesOrderNumberWithFixedDate` | 주문번호 `ORD-YYYYMMDD-NNNN` 형식 생성(고정 Clock) |
| `OrderNumberSequenceIncrementsForSameDate` | 같은 날짜 내 순번이 1씩 증가 |
| `RejectsUnregisteredSampleId` | 미등록 시료 ID 주문 거부 |
| `RejectsNonPositiveQuantityFromDummyInvalidSet` | dummygen invalid 세트 중 수량≤0 케이스 거부 |
| `RejectsEmptyCustomerName` | 빈 고객명 거부 |
| `ApproveWithSufficientStockDeductsAndConfirms` | 재고 충분 시 즉시 차감 + CONFIRMED |
| `ApproveWithInsufficientStockZeroesStockAndProduces` | 재고 부족 시 전량 점유(0) + 부족분 계산 + PRODUCING |
| `ApproveExactStockMatchConfirmsWithoutShortage` | **경계값**: 재고==주문량이면 "충분" 분기(CONFIRMED), "부족" 아님 |
| `RejectTransitionsToRejectedWithoutStockChange` | 거절 시 REJECTED 전환, 재고 변경 없음 |
| `RejectsReapprovalOfNonReservedOrder` | 이미 처리된 주문의 재승인/재거절 거부 |
| `ApproveUnknownOrderNumberFails` | 존재하지 않는 주문번호 승인 실패 |
| `ListByStatusReturnsOnlyMatchingOrders` | 상태별 필터링 조회(RESERVED/CONFIRMED 등) |
| `ApprovalPersistsAcrossReload` | 승인 후 재시작해도 상태·부족분·재고 유지 |
| `ReleaseTransitionsConfirmedToReleaseWithoutStockChange` | 출고 시 CONFIRMED→RELEASE 전환, 재고 변경 없음(승인 시점에 이미 반영) |
| `RejectsReleaseOfNonConfirmedOrders` | RESERVED/REJECTED/PRODUCING/이미 RELEASE 상태의 주문 출고 거부 |
| `ReleaseUnknownOrderNumberFails` | 존재하지 않는 주문번호 출고 실패 |
| `ReleasePersistsAcrossReload` | 출고 후 재시작해도 RELEASE 상태·재고 유지 |
| `PersistsAcrossReload` | 예약(RESERVED) 후 재시작해도 조회 가능 |

### `OrderControllerTest.cpp` — Phase 3/4/6 (Controller ↔ View 위임)

| 테스트 | 검증 내용 |
|---|---|
| `ReserveOrderShowsSuccessMessage` | 주문 접수 성공 메시지 전달 |
| `ReserveOrderForUnknownSampleShowsError` | 미등록 시료 주문 시 오류 전달 |
| `ListOrdersForwardsToView` | 주문 목록을 View에 전달 |
| `ApproveOrderShowsSuccessMessage` | 승인 성공 메시지 전달(충분/부족 공통) |
| `ApproveUnknownOrderShowsError` | 존재하지 않는 주문 승인 시 오류 전달 |
| `RejectOrderShowsSuccessMessage` | 거절 성공 메시지 전달 |
| `ListReservedOrdersForwardsOnlyReservedToView` | RESERVED만 필터링해 View에 전달 |
| `ReleaseOrderShowsSuccessMessage` | 출고 성공 메시지 전달 |
| `ReleaseNonConfirmedOrderShowsError` | CONFIRMED가 아닌 주문 출고 시도 시 오류 전달 |
| `ListConfirmedOrdersForwardsOnlyConfirmedToView` | CONFIRMED만 필터링해 View에 전달(출고 완료된 주문은 제외) |

### `ProductionQueueTest.cpp` — Phase 5 (생산 큐)

| 테스트 | 검증 내용 |
|---|---|
| `ApprovedInsufficientOrdersStayInWaitingQueueFifo` | 승인(부족)된 주문들이 대기 큐에 FIFO 순서로 유지 |
| `AdvanceStartsFirstWaitingOrderWhenNoActiveProduction` | 활성 생산 없을 때 대기 큐 맨 앞 주문의 생산 시작(수량/완료시각 계산) |
| `RemainsProducingBeforeCompletionAndConfirmsAfter` | 완료 시각 이전 PRODUCING 유지, 이후 CONFIRMED+재고 batch 반영 |
| `ProcessesMultipleWaitingOrdersSequentiallyWithoutMerging` | 여러 대기 주문이 병합 없이 순차 처리 |
| `CompletesImmediatelyOnRestartAfterCompletionTimePassed` | 재시작 시나리오(생명주기 무관 완료 판정) |
| `TC1_SufficientStockCoversBothOrders_NoProductionForEither` | 재고 여유 시 두 주문 모두 생산 없이 처리 (2부 TC-1) |
| `TC2_PartialStockReducesShortage_SecondOrderQueuesWhileFirstActive` | 재고 일부 부족 + 두 번째 주문이 활성 생산 중 대기 (2부 TC-2) |
| `TC3_OrderApprovedAfterPriorCompletion_EvaluatesAgainstUpdatedStockIndependently` | 선행 생산 완료 이후 도착한 주문의 독립 평가 (2부 TC-3) |
| `TC4_ThreeOrdersMaintainStrictFifoOrderWithoutMerging` | 대기 주문 3건의 FIFO 순차 처리 (2부 TC-4) |
| `TC5_ProductionQuantityRoundsUpForNonDivisibleShortage` | 수율 나눗셈 ceil 올림 (2부 TC-5) |
| `TC6_RestartAfterCompletionTimePersistsConfirmedStateToRawJsonFile` | 재시작 후 완료 상태의 원본 파일 영속화 확인 (2부 TC-6) |

### `MonitoringServiceTest.cpp` — Phase 7 (주문량/재고량 집계)

| 테스트 | 검증 내용 |
|---|---|
| `OrderCountSummaryExcludesRejected` | 상태별 주문 건수 집계, REJECTED는 어느 카운트에도 포함되지 않음 |
| `InventoryStatusDepletedWhenStockIsZeroRegardlessOfReserved` | 재고 0이면 RESERVED 주문 유무와 무관하게 "고갈"(최우선 판정) |
| `InventoryStatusPlentyWhenStockEqualsReservedSum` | **경계값**: 재고==RESERVED 합이면 "여유" |
| `InventoryStatusLowWhenStockBelowReservedSum` | 재고 < RESERVED 합이면 "부족" |
| `InventoryStatusIgnoresNonReservedOrdersInComparison` | CONFIRMED/PRODUCING 등은 재고 판정 비교 대상에서 제외(RESERVED만 합산) |
| `InventoryLevelToStringMapsAllLevels` | 여유/부족/고갈 문자열 매핑 |

### `MonitoringControllerTest.cpp` — Phase 7 (Controller ↔ View 위임)

| 테스트 | 검증 내용 |
|---|---|
| `ShowOrderSummaryForwardsToView` | 주문량 집계 결과를 View에 전달 |
| `ShowInventoryStatusForwardsToView` | 재고 상태 목록을 View에 전달 |

## 2부. 재고/타이밍 시나리오 상세 (승인·생산 큐 심화)

## 배경 규칙 (요약)

- 재고 `stock >= 주문수량` → 즉시 차감, CONFIRMED (생산 없음)
- 재고 `stock < 주문수량` → 기존 재고 전량 즉시 점유(0으로 차감), 부족분 = 주문수량 - 재고, PRODUCING
- 실 생산량 = `ceil(부족분 / 수율)`, 총 생산시간 = `평균생산시간(분) * 실 생산량`
- 생산은 **주문 승인 시점이 아니라 큐 시작 시점**에 `productionQuantity`/시작·완료 시각이 계산되어 고정됨(재계산 없음)
- 생산 큐는 **활성 생산 1건 제한**, 병합 없이 FIFO 순차 처리
- 생산 완료 판정은 **저장된 시각과 호출 시점의 시각 비교**로만 이루어지며 프로그램 생명주기와 무관

## 시나리오 매트릭스

| TC | 시나리오 | 초기 재고 | 주문A 수량 | 주문B(또는 C) 수량/타이밍 | 대응 테스트 |
|---|---|---|---|---|---|
| TC-0a | 재고 == 주문량 (경계값) | 50 | 50 | - | `OrderRepositoryTest.ApproveExactStockMatchConfirmsWithoutShortage` |
| TC-0b | 재고 < 주문량 | 30 | 200 | - | `OrderRepositoryTest.ApproveWithInsufficientStockZeroesStockAndProduces` |
| TC-1 | 재고가 두 주문 합계보다 많음(여유) | 200 | 50 | 50 (A 직후 즉시) | `ProductionQueueTest.TC1_SufficientStockCoversBothOrders_NoProductionForEither` |
| TC-2 | 재고가 주문A보다 적음, B는 A 생산 중 도착 | 10 | 50 | 50 (A 생산 중 t=5분) | `ProductionQueueTest.TC2_PartialStockReducesShortage_SecondOrderQueuesWhileFirstActive` |
| TC-3 | B가 A 생산 **완료 이후** 도착 | 10 | 50 | 30 (A 완료 후 t=20분) | `ProductionQueueTest.TC3_OrderApprovedAfterPriorCompletion_EvaluatesAgainstUpdatedStockIndependently` |
| TC-4 | 대기 주문 3건, FIFO 순차 처리 | 0 | 5 | 10, 15 (순차 접수) | `ProductionQueueTest.TC4_ThreeOrdersMaintainStrictFifoOrderWithoutMerging` |
| TC-5 | 부족분이 수율로 나누어떨어지지 않음(올림) | 0 | 7 | - (수율 0.3) | `ProductionQueueTest.TC5_ProductionQuantityRoundsUpForNonDivisibleShortage` |
| TC-6 | 생산 중 프로세스 종료 → 완료시각 이후 재시작 | 0 | 20 | - | `ProductionQueueTest.TC6_RestartAfterCompletionTimePersistsConfirmedStateToRawJsonFile` |
| (참고) | 앞서 대화에서 다룬 원본 두 케이스(재고 50 / 재고 10, 5분 간격 두 주문) | 50, 10 | 50 | 50 | TC-1, TC-2와 동일 로직의 초기 탐색 사례 (수동 트레이스로 먼저 확인 후 TC-1/TC-2로 일반화) |

## TC별 상세

### TC-0a. 재고 == 주문량 (경계값)

`approve()`의 분기 조건은 `stock >= quantity`이므로, 정확히 같을 때도 "부족"이 아니라 "충분"으로 처리되어야 한다.

- 전제: 시료 재고 50, 주문 수량 50
- 결과: 즉시 CONFIRMED, `shortageQuantity=0`, 재고 0 (생산 없음)

### TC-1. 재고 여유 — 두 주문 모두 생산 없이 처리

- 전제: 재고 200, 주문A 50, 주문B 50 (각각 승인 즉시)
- 타임라인:
  | 시점 | 이벤트 | 재고 |
  |---|---|---|
  | 시작 | - | 200 |
  | 주문A 승인 | 재고>=50 → 즉시 CONFIRMED | 150 |
  | 주문B 승인 | 재고>=50 → 즉시 CONFIRMED | 100 |
- `advance()`를 호출해도 활성 생산/대기 큐 모두 비어 있음 (생산 큐가 전혀 관여하지 않음)

### TC-2. 재고 부족 — 부족분 계산 + 활성 생산 중 대기

- 전제: 재고 10, 수율 0.5, 평균생산시간 0.2min/ea, 주문A 50(t=0), 주문B 50(t=5분, A 생산 중)
- 타임라인:
  | 시점 | 이벤트 | 재고 | 비고 |
  |---|---|---|---|
  | t=0 | 주문A 승인 | 0 | 부족분=40 (10 전량 점유) |
  | t=0 | A 생산 시작 | 0 | productionQuantity=ceil(40/0.5)=**80**, 완료예정 t=16분 |
  | t=5분 | 주문B 승인 | 0 | 부족분=50 (재고 이미 0). **A가 활성 상태라 B는 대기 큐에만 들어감** |
  | t=16분 | A 완료 + B 생산 시작(한 번의 advance()에서 순차) | **80** | A의 productionQuantity(80) batch 반영. B: productionQuantity=ceil(50/0.5)=**100** |

### TC-3. 뒤늦게 도착한 주문은 최신 재고로 독립 평가

- 전제: TC-2와 동일 설정, 단 C는 **A 생산 완료 이후**(t=20분)에 접수+승인
- 타임라인:
  | 시점 | 이벤트 | 재고 |
  |---|---|---|
  | t=16분 | A 완료 | 80 |
  | t=20분 | 주문C(30) 승인 | 재고 80>=30 → **생산 없이 즉시 CONFIRMED**, 재고 **50** |
- 시사점: 생산량/부족분은 **승인 시점의 재고**로만 결정되고 재계산되지 않으므로, 같은 수량이라도 도착 타이밍에 따라 생산이 필요할 수도, 필요 없을 수도 있다.

### TC-4. 대기 주문 3건 — FIFO 순차, 병합 없음

- 전제: 재고 0, 수율 1.0, 평균생산시간 1.0min/ea. 주문A(5), B(10), C(15) 순서로 접수+승인
- 결과: A(5분)→B(10분)→C(15분) 순서로 **한 번에 하나씩만** 생산되며, 대기 큐 크기는 2→1→0으로 줄어듦. 어떤 시점에도 두 주문이 하나로 합쳐져 생산되지 않음

### TC-5. 수율 나눗셈 올림(ceil) 확인

- 전제: 재고 0, 수율 0.3, 주문 7 → 부족분 7
- 결과: `실 생산량 = ceil(7/0.3) = ceil(23.33...) = 24` (23으로 내림하지 않음)

### TC-6. 데이터 영속성 — 생산 중 종료 후, 완료 시각 이후 재시작 시 완료 반영 확인

사용자 요청으로 추가된 케이스: "주문이 들어간 뒤 프로그램이 종료되고, 실제 생산 완료 시점 이후에
프로그램이 다시 켜졌을 때 생산 완료가 확인되는지"를 검증한다.

- 전제: 재고 0, 수율 0.5, 평균생산시간 1.0min/ea, 주문 20 → 부족분 20 → productionQuantity=ceil(20/0.5)=40, 생산시간 40분
- 절차:
  1. 주문 접수 + 승인 (PRODUCING 전환) 후 `advance()` 1회로 생산 시작
  2. **프로세스 종료를 시뮬레이션**: `JsonStore`/`OrderRepository`/`ProductionQueue` 인스턴스를 스코프 밖으로 소멸시킴 (파일에는 저장된 상태만 남음)
  3. 종료 시점에 저장된 원본 JSON 파일을 **직접 열어** `status: "PRODUCING"`, `productionCompletesAtEpochSec` 존재를 확인 (아직 완료 전임을 재확인)
  4. "재시작": 완료 예정 시각을 24시간 지난 시각을 반환하는 새 Clock으로 `JsonStore`를 파일에서 다시 로드하고 `OrderRepository`/`ProductionQueue`를 새로 구성
  5. `advance()`를 **1회만** 호출
  6. API(`OrderRepository::find`)로 `CONFIRMED` 확인, 재고 40 확인
  7. **원본 JSON 파일을 다시 직접 열어** `status: "CONFIRMED"`, `samples[0].stock: 40`이 실제로 디스크에 저장되었는지 재확인 (메모리 캐시만 갱신되고 파일 저장이 누락되는 회귀를 잡기 위함)
- 핵심 확인 사항: 생산 완료 판정은 프로세스가 계속 실행 중이었는지와 무관하게, **오직 저장된 시각과 재시작 시점의 현재 시각 비교**만으로 이루어진다.
- 참고: 콘솔 앱으로도 동일 시나리오를 실제 wall-clock(5초) 대기 후 프로세스 재시작으로 시연한 바 있다 (`log/phase5.md`의 Demonstrability 절 참고). 이 TC는 그 수동 시연을 임의 시간 지연(24시간)까지 포함해 자동화한 것이다.

## 실행 방법

```
bash scripts/build.sh
ctest --test-dir build --output-on-failure -R "ProductionQueueTest|OrderRepositoryTest"
```

## 커버리지 메모

- 대칭적으로 "재고가 정확히 0인" 극단값은 TC-2/TC-4/TC-5/TC-6에서 이미 다뤄짐.
- "여러 시료(Sample)에 걸친 동시 생산" 케이스는 다루지 않음 — `prd.md` §3.4에서 생산 라인은 단일 라인이며, 재고/생산 큐는 시료 단위가 아니라 전체 주문 단위로 공유되는 하나의 큐이므로 시료가 달라도 큐 자체는 하나다(범위 밖 아님, 다만 이번 카탈로그에서 별도 케이스로 다루지는 않음).
- 승인 자체가 거부되는 케이스(존재하지 않는 시료/주문번호, 이미 처리된 주문 재승인 등)는 Phase 4 테스트(`OrderRepositoryTest`)에서 이미 다루고 있어 이 카탈로그에서는 재고/타이밍 조합에 집중했다.
