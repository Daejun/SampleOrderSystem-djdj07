# Phase 8 구현 완료 및 Verify 결과

이 프로젝트의 마지막 Phase다.

- 관련 설계 문서: `docs/design/phase8.md`
- 관련 계획: `PLAN.md` Phase 8

## 구현 내용

| 파일 | 내용 |
|---|---|
| `src/model/MonitoringService.h/.cpp` | `MainMenuSummary`(등록 시료 수/총 재고/전체 주문 수(REJECTED 포함)/생산라인 대기 건수), `mainMenuSummary()` 추가. 새 클래스 신설 없이 Phase 7 `MonitoringService` 확장 |
| `src/view/IView.h`, `ConsoleView.h/.cpp` | `showMainMenu` 추가 |
| `src/main.cpp` | `printMenu()` 자유 함수 제거 → `view.showMainMenu(monitoringService.mainMenuSummary())`로 대체. 메인 메뉴 루프 **매 반복마다** `productionQueue.advance()` 호출 추가 (생산 라인 조회 메뉴를 거치지 않아도 완료가 지연 없이 반영되도록) |
| `tests/MonitoringServiceTest.cpp` | `mainMenuSummary()` 케이스 추가 — REJECTED 포함 전체 카운트, `producingCount`가 `orderCountSummary().producing`과 동일한 값을 재사용하는지 확인 |
| `tests/EndToEndTest.cpp` | 신규. Repository/Service 계층을 직접 연결해 접수→승인(부족)→생산 시작·완료→출고→최종 집계까지 전체 흐름을 한 번에 검증하는 회귀 테스트 |
| `tests/StubView.h` | `showMainMenu` 추가 |
| `tc.md` | 1부 인벤토리에 신규 테스트 2개 반영 |
| `PLAN.md` | Phase 8 구현 완료 노트 추가 |

## Verify 결과

### 1. Test Verify — `scripts/build.sh`

```
$ bash scripts/build.sh
...
[100%] Built target sample_order_tests
...
Test project C:/Users/User/semi/build
 (70개 테스트 전체)

100% tests passed, 0 tests failed out of 70
```

결과: **통과 (70/70, Phase 1~7의 기존 68개 포함 회귀 없음)**. 이번 Phase는 Phase 6/7에서와 달리
구현 중 테스트 실패 없이 첫 시도에 통과했다 (재고 소진 순서에 유의해 테스트를 작성함). 임의 완화/우회
없음.

### 2. Compliance Verify — `docs/design/phase8.md` / `PLAN.md` Phase 8 요구사항 대조

| 요구사항 | 충족 여부 |
|---|---|
| 메인 메뉴에 등록 시료 수/총 재고/전체 주문 수(REJECTED 포함)/생산라인 대기 건수 정확히 표시 | O (`MainMenuSummaryCountsAllOrdersIncludingRejectedAndReusesProducing`) |
| 메인 메뉴 출력 시마다 `advance()` 호출로 생산 완료 지연 없이 반영 | O (콘솔 시연: "생산 라인 조회" 메뉴를 한 번도 방문하지 않고도 재고·대기건수 자동 갱신) |
| `EndToEndTest`로 "접수→승인(부족)→생산 시작→완료(CONFIRMED)→출고(RELEASE)" 전체 흐름 한 번에 통과 | O (`FullOrderJourneyFromReservationToRelease`) |
| `ctest` 전체 통과 | O (70/70) |
| 콘솔 실행 로그로 전체 흐름 + 메인 메뉴 요약 변화 재현 | O (아래 Demonstrability) |

### 3. Document Consistency Verify

구현 전 `CLAUDE.md`/`prd.md`/`PLAN.md`/`docs/design/phase8.md` 상호 참조 확인. `prd.md` §4.1(메인 메뉴
요약)과 §4.5(모니터링 REJECTED 제외) 간의 잠재적 모호함은 설계 문서에서 "합리적 해석(REJECTED 포함)"으로
근거를 명시하고 사람 확인 없이 진행하도록 이미 합의되어 있었다. 추가 충돌 없음.

### 4. Demonstrability — 콘솔 실행 로그 (전체 흐름 + 메인 메뉴 요약 변화)

```
$ ./build/src/sample_order_app.exe   # 최초 실행 (빈 상태)
=== 반도체 시료 생산주문관리 시스템 ===
등록 시료 0종  총 재고 0 ea  전체 주문 0건  생산라인 대기 0건

선택 > 1  # 시료 등록 (S-001, 평균생산시간 0.0166min/ea, 수율 1.0, 재고 0)
...
등록 시료 1종  총 재고 0 ea  전체 주문 0건  생산라인 대기 0건

선택 > 2  # 주문 접수 (5ea)
...
등록 시료 1종  총 재고 0 ea  전체 주문 1건  생산라인 대기 0건

선택 > 3  # 승인 -> 재고 부족, PRODUCING
주문 승인 완료 (재고 부족, 생산 대기): ORD-20260715-0001
...
등록 시료 1종  총 재고 0 ea  전체 주문 1건  생산라인 대기 1건

선택 > 0  # 종료 (아직 생산 시작 전, 큐에 대기만 있는 상태)
```

```
$ ./build/src/sample_order_app.exe   # 재실행 (여기서 advance()가 생산을 시작시킴)
# (수 초 실제 대기 후 다시 실행 — "생산 라인 조회" 메뉴는 한 번도 방문하지 않음)
=== 반도체 시료 생산주문관리 시스템 ===
등록 시료 1종  총 재고 5 ea  전체 주문 1건  생산라인 대기 0건   # <- 생산이 이미 자동 완료됨(재고 5, 대기 0건)

선택 > 6  # 출고 처리
[ORD-20260715-0001] S-001 x 5 ea - 고객 (CONFIRMED)
선택 > 2
주문번호 > ORD-20260715-0001
출고 처리 완료: ORD-20260715-0001

선택 > 0
등록 시료 1종  총 재고 5 ea  전체 주문 1건  생산라인 대기 0건   # 출고는 재고를 건드리지 않음
```

`data/data.json` 내용 (최종):
```json
{
  "samples": [
    { "id": "S-001", "name": "테스트", "avgProductionTimeMinutes": 0.0166, "yield": 1.0, "stock": 5 }
  ],
  "orders": [
    { "orderNumber": "ORD-20260715-0001", "sampleId": "S-001", "customerName": "고객",
      "quantity": 5, "shortageQuantity": 5, "productionQuantity": 5, "status": "RELEASE",
      "productionStartedAtEpochSec": 1784090673, "productionCompletesAtEpochSec": 1784090677 }
  ]
}
```

**핵심 확인 사항**: "생산 라인 조회"(메뉴 5)를 한 번도 방문하지 않았음에도, 메인 메뉴에 돌아올 때마다
`advance()`가 호출되어 재고(0→5)와 생산라인 대기 건수(1→0)가 자동으로 갱신되었다 — 이번 Phase의
핵심 요구사항이 실제로 동작함을 확인했다.

재현 커맨드:
```
bash scripts/build.sh
./build/src/sample_order_app.exe
```

## 검증하지 못한 내용

- `MainController`의 각 `runXxxMenu()`(`std::cin` 기반 입력 루프) 자체에 대한 자동화 테스트는 Phase 2부터
  일관되게 미검증 범위로 유지했다 — `EndToEndTest`는 Controller 계층을 우회해 Repository/Service를
  직접 검증하므로 이 범위를 메우지 않는다.
- 극단적으로 짧은 시간 내 다수의 동시 프로세스 실행(진짜 동시성) 시나리오는 다루지 않음 — 콘솔 단일
  사용자·단일 프로세스 가정(prd.md 범위)이므로 해당 없음.

## 프로젝트 완료 메모

Phase 1~8 전체가 완료되어 `PLAN.md`에 정의된 모든 Phase의 구현·Verify·문서화가 끝났다. 최종 테스트
현황은 70/70 통과이며, `tc.md`가 전체 테스트 케이스 인벤토리와 재고/타이밍 심화 시나리오를 갈무리하고
있다.
