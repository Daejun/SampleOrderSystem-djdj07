# Phase 5 구현 완료 및 Verify 결과

- 관련 설계 문서: `docs/design/phase5.md`
- 관련 계획: `PLAN.md` Phase 5

## 구현 내용

| 파일 | 내용 |
|---|---|
| `src/model/Order.h` | `productionQuantity`, `productionStartedAtEpochSec`(`optional<int64_t>`), `productionCompletesAtEpochSec`(`optional<int64_t>`) 추가 |
| `src/model/OrderJson.h/.cpp` | 신규. `OrderRepository`/`ProductionQueue`가 공유하는 Order↔JSON 변환 (필드 중복 유지 방지). 신규 필드는 `.value()` 기본값으로 Phase 3~4 데이터도 하위 호환 |
| `src/model/OrderRepository.cpp` | 로컬 `fromJson`/`toJson`을 `OrderJson.h`의 공유 함수로 교체, `approve()`가 `setStock` 대신 `adjustStockInMemory` + 단일 `store_.save()` 사용하도록 리팩터링 (Phase 4 코드 리뷰 반영) |
| `src/model/SampleRepository.h/.cpp` | `adjustStockInMemory`(저장 없음) 신설, `setStock`은 이를 이용해 재정의 (동작 동일) |
| `src/model/ProductionQueue.h/.cpp` | 신규. `advance()`(완료 판정+batch 반영, 다음 생산 시작), `waitingQueue()`, `activeProduction()` |
| `src/controller/ProductionController.h/.cpp` | 신규. `showStatus()` — `advance()` 호출 후 현황을 View에 전달 |
| `src/controller/MainController.h/.cpp` | 생성자에 `ProductionController&` 추가, "5" 선택 시 `productionController_.showStatus()` 호출 |
| `src/view/IView.h`, `ConsoleView.h/.cpp` | `showProductionStatus` 추가 |
| `src/main.cpp` | `ProductionQueue`/`ProductionController` 생성·연결, `store.ensureLoaded()` 직후 `advance()` 1회 호출(재시작 시 밀린 완료 즉시 반영) |
| `tests/ProductionQueueTest.cpp` | 신규. FIFO 대기, 생산 시작(수량/완료시각 계산), 완료 전/후 상태·재고, 여러 주문 순차 처리(병합 없음), 재시작 시나리오(가변 Clock으로 시간 경과 시뮬레이션) |
| `tests/OrderRepositoryTest.cpp`, `OrderControllerTest.cpp` | 기존 테스트 회귀 확인 (신규 필드/리팩터링 이후에도 그대로 통과) |
| `tests/StubView.h` | `showProductionStatus`, `lastActiveProduction`/`lastWaitingQueue` 추가 |

## Verify 결과

### 1. Test Verify — `scripts/build.sh`

빌드 중 `ProductionQueueTest.RemainsProducingBeforeCompletionAndConfirmsAfter`가 1회 실패했다.
원인은 구현 버그가 아니라 **테스트 설정 실수**였다: `SampleRepository::registerSample()`은 Phase 2
설계대로 등록 시 재고를 항상 0으로 강제하는데, 테스트에서 `registerSample({..., stock=10})`으로 넘긴
초기 재고 10이 무시된 채 실제 재고는 0으로 등록되었다. 이 때문에 부족분이 의도한 50이 아니라 60으로
계산되어 총 생산시간이 3600초였는데, 테스트는 3000초만 경과시키고 완료를 기대했다. 디버그용 스크래치
프로그램으로 실제 필드 값을 직접 출력해 원인을 특정한 뒤, 테스트에 `sampleRepo.setStock("S-001", 10)`
호출을 추가해(Phase 4 테스트들과 동일한 패턴) 수정했다.

```
$ bash scripts/build.sh
...
[100%] Built target sample_order_tests
...
Test project C:/Users/User/semi/build
 (ProductionQueueTest 5개, OrderControllerTest 7개, OrderRepositoryTest 13개, OrderInputTest 5개,
  SampleControllerTest 5개, SampleRepositoryTest 7개, JsonStoreTest 3개, Placeholder 1개)

100% tests passed, 0 tests failed out of 46
```

결과: **통과 (46/46, Phase 1~4의 기존 41개 포함 회귀 없음)**. 실패했던 테스트는 원인 파악 후 수정하여
재실행으로 통과를 확인했으며, 구현 코드를 임의로 완화하지 않았다.

### 2. Compliance Verify — `docs/design/phase5.md` / `PLAN.md` Phase 5 요구사항 대조

| 요구사항 | 충족 여부 |
|---|---|
| 승인(재고부족)된 주문이 대기 큐에 FIFO 순서로 남음 | O (`ApprovedInsufficientOrdersStayInWaitingQueueFifo`) |
| `advance()` 시 활성 생산 없으면 대기 큐 맨 앞 주문 시작(수량/완료시각 계산) | O (`AdvanceStartsFirstWaitingOrderWhenNoActiveProduction`) |
| 완료 시각 이전 PRODUCING 유지, 이후 CONFIRMED+재고 batch 반영 | O (`RemainsProducingBeforeCompletionAndConfirmsAfter`) |
| 여러 대기 주문이 병합 없이 개별 순차 처리 | O (`ProcessesMultipleWaitingOrdersSequentiallyWithoutMerging`) |
| 고정 Clock으로 재시작 시나리오(생명주기 무관 판정) 검증 | O (`CompletesImmediatelyOnRestartAfterCompletionTimePassed`) |
| `Sample.stock`이 PRODUCING 동안 가용 재고에서 제외 | O (Phase 4에서 이미 0으로 차감되어 있으므로 자연히 만족, 완료 전 테스트로 재확인) |
| `OrderRepository::approve()` 리팩터링 후 회귀 없음 | O (Phase 4 테스트 13개 그대로 통과) |
| `ctest` 전체 통과 | O (46/46) |
| 콘솔 실행 로그로 "승인(부족)→생산 조회(대기)→(실시간 경과)→생산 조회(완료)" 재현 | O (아래 Demonstrability) |

### 3. Document Consistency Verify

구현 전 `CLAUDE.md`/`prd.md`/`PLAN.md`/`docs/design/phase5.md` 상호 참조 확인. PLAN.md의 마지막
미결 사항(생산 완료 시각 판정 방식)이 이번 Phase로 해소되었음을 확인했고, 추가 충돌 없음.

### 4. Demonstrability — 콘솔 실행 로그 (실시간 생산 완료)

평균 생산시간을 0.0166분(≈1초)으로 등록해 실제 wall-clock 경과로 생산 완료를 시연했다.

```
$ ./build/src/sample_order_app.exe
...
선택 > 1  -> 시료 등록 (S-001, 평균생산시간 0.0166 min/ea, 수율 1.0, 재고 0)
...
선택 > 2  -> 주문 접수 (수량 5)
주문 접수 완료: ORD-20260715-0001

선택 > 3  -> 승인
주문 승인 완료 (재고 부족, 생산 대기): ORD-20260715-0001

선택 > 5  -> 생산 라인 조회 (승인 직후)
현재 생산 중: [ORD-20260715-0001] S-001, 생산량 5 ea
대기 중인 주문이 없습니다.

$ sleep 5   # 실제 5초 대기 (wall-clock)

$ ./build/src/sample_order_app.exe   # 새 프로세스로 재시작
선택 > 5  -> 생산 라인 조회
현재 생산 중인 주문이 없습니다.
대기 중인 주문이 없습니다.
```

`data/data.json` (완료 후):
```json
{
  "samples": [
    { "id": "S-001", "name": "테스트 시료", "avgProductionTimeMinutes": 0.0166, "yield": 1.0, "stock": 5 }
  ],
  "orders": [
    {
      "orderNumber": "ORD-20260715-0001", "sampleId": "S-001", "customerName": "고객",
      "quantity": 5, "shortageQuantity": 5, "productionQuantity": 5, "status": "CONFIRMED",
      "productionStartedAtEpochSec": 1784087160, "productionCompletesAtEpochSec": 1784087164
    }
  ]
}
```

`productionCompletesAtEpochSec`(1784087164) - `productionStartedAtEpochSec`(1784087160) = 4초로,
`ceil(5/1.0)=5 ea` 생산량에 `0.0166min/ea * 60 * 5 ≈ 4.98초`가 반영된 값이다. 별도 프로세스 재시작(새
`sample_order_app.exe` 실행) 직후 `advance()` 1회만으로 완료 판정된 것을 확인해, 프로그램 생명주기와
무관하게 저장된 시각만으로 판정한다는 규칙을 실제로 검증했다.

재현 커맨드:
```
bash scripts/build.sh
./build/src/sample_order_app.exe   # 등록 -> 주문 -> 승인 -> 생산 라인 조회
sleep 5
./build/src/sample_order_app.exe   # 재시작 -> 생산 라인 조회 (완료 확인)
```

## 검증하지 못한 내용

- 여러 생산 라인 동시 운영은 prd.md에서 명시적으로 범위 밖(단일 라인)이므로 다루지 않음.
- `MainController`의 `std::cin` 기반 입력 루프 자체에 대한 자동화 테스트는 여전히 작성하지 않음(콘솔 실행 로그로만 확인) — 이전 Phase와 동일한 미검증 범위.
