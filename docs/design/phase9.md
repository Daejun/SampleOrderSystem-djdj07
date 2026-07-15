# Phase 9 설계: 코드 정리 — `main.cpp` 중복 `advance()` 호출 제거

`PLAN.md`의 Phase 9 목표(중복 호출 제거)에 대한 상세 설계. 새 기능 추가나 동작 변경이 없는 **순수 클린업
Phase**다.

## 배경

`docs/design/phase8.md`/`log/phase8.md` 검토 중 발견된 사항이다. 현재 `main.cpp`:

```cpp
ProductionQueue productionQueue(store, sampleRepository, orderRepository);
productionQueue.advance();  // (A) Phase 5: 재시작 시 밀린 생산완료를 즉시 반영

MonitoringService monitoringService(sampleRepository, orderRepository);
...
while (!controller.isExitRequested()) {
    productionQueue.advance();  // (B) Phase 8: 생산 라인 조회를 거치지 않아도 완료가 지연 없이 반영되도록
    view.showMainMenu(monitoringService.mainMenuSummary());
    ...
}
```

(A)는 Phase 5 시점에는 "메인 루프가 매번 `advance()`를 호출하지 않았기 때문에" 필요했던 유일한 시작 시점
갱신 지점이었다. Phase 8에서 (B)가 추가되면서, 루프의 첫 반복이 화면을 그리기 전에 (A)와 동일한 일을
다시 수행한다 — 그 사이에는 시간이 전혀 흐르지 않으므로 (A)는 이제 실질적으로 아무 효과가 없는 중복
호출이다. 정확성 문제는 없지만(멱등적이라 안전), 두 번 호출하는 이유가 코드만 봐서는 불분명해 가독성을
해친다.

## 변경 사항

- `main.cpp`에서 (A) 줄과 그 주석(`// 재시작 시 밀린 생산완료를 즉시 반영`)을 삭제한다.
- (B)는 그대로 유지한다 — 루프 진입 시점(첫 반복 포함)에 항상 먼저 실행되므로, (A)가 하던 역할을 이미
  완전히 포함한다.
- `ProductionQueue`/`MonitoringService` 등 다른 컴포넌트나 시그니처는 전혀 바꾸지 않는다.

## 영향 범위 확인

- `tests/ProductionQueueTest.cpp`, `tests/EndToEndTest.cpp` 등 기존 테스트는 `main.cpp`를 거치지 않고
  `ProductionQueue`를 직접 생성해 `advance()`를 호출하므로, 이번 변경의 영향을 받지 않는다.
- `main.cpp`를 직접 실행하는 콘솔 시나리오만 영향을 받으며, (B)가 (A)를 완전히 대체하므로 동작 변화가
  없어야 한다.

## Phase 9에서 하지 않는 것

- 새 기능, 새 검증 로직 추가 없음
- `ProductionQueue::advance()`의 내부 로직 변경 없음 (Phase 5에서 이미 확정된 규칙 그대로)

## 완료 기준

- 기존 70개 테스트 전체 회귀 없이 통과 (`ctest`)
- Phase 8 Demonstrability에서 확인했던 시나리오("생산 라인 조회"를 방문하지 않고도 메인 메뉴 재진입 시
  재고·생산라인 대기 건수가 자동 갱신됨)를 동일하게 재현해, (A) 제거가 그 동작에 영향을 주지 않았음을
  콘솔 실행 로그로 재확인
- `main.cpp`에 중복 호출/불명확한 주석이 남아있지 않은지 육안 확인
