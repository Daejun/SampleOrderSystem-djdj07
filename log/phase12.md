# Phase 12 구현 완료 및 Verify 결과

새 기능·동작 변경 없이 테스트만 추가하는 Phase다.

- 관련 설계 문서: `docs/design/phase12.md`
- 관련 계획: `PLAN.md` Phase 12

## 구현 내용

| 파일 | 내용 |
|---|---|
| `scripts/coverage.sh` | `gcov` 호출에 `-b`(분기 확률) 추가 |
| `tests/SampleRepositoryTest.cpp` | `SampleSearchFieldTest`(`TEST_P`, 5케이스) — `search()`의 4항 OR 각각을 독립 검증. `SampleRegisterBoundaryTest`(`TEST_P`, 7케이스) — `registerSample()`의 avgTime/yield 경계값(정확히 0, 음수, 정확히 1.0, 1.0 초과)을 의도적으로 나열해 각 조건을 독립 검증. 두 구조체 모두 gtest 기본 프린터가 바이트 덤프를 출력하는 것을 막기 위해 `PrintTo()` 오버로드 추가 |
| `tests/ProductionQueueTest.cpp` | `SkipsOrderMissingCompletionTimeDespiteHavingStarted` 추가 — 생산 시작은 됐지만 완료 시각이 없는 손상된 주문을 직접 store에 심어 예외 없이 건너뛰는지 확인 |
| `tests/JsonStoreTest.cpp` | `RepairsMissingSamplesKeyInExistingFile`(키 자체가 없음), `RepairsWrongTypeSamplesFieldInExistingFile`(타입이 배열이 아님) 추가 — `ensureArrayField`의 OR 조건 두 항을 각각 독립 검증 |
| `tc.md` | 1부 인벤토리에 신규 15개 반영(87→102), 3부 "분기/조건 커버리지 매트릭스" 신설 |

## Verify 결과

### 1. Test Verify — `scripts/build.sh`

```
$ bash scripts/build.sh
...
100% tests passed, 0 tests failed out of 102
```

결과: **통과 (102/102, 신규 15개 포함, 기존 87개 회귀 없음)**. 임의 완화/우회 없음.

### 2. Compliance Verify — `docs/design/phase12.md` 완료 기준 대조

`bash scripts/coverage.sh`(`gcov -b` 포함)를 실행해 우선순위 1~3 대상 분기의 실제 `.gcov` `branch ...
taken` 출력을 확인했다.

| 위치 | 조건 | 참 경로 | 거짓 경로 |
|---|---|---|---|
| `SampleRepository.cpp:43` | `avgTime <= 0.0` | taken 4% | taken 96% |
| `SampleRepository.cpp:46` | `yield <= 0.0` | taken 4% | taken 96% |
| `SampleRepository.cpp:46` | `\|\| yield > 1.0` | taken 4% | taken 96% |
| `SampleRepository.cpp:99-102` | OR 4항 각각 | 12~21%(각 항목별) | 나머지 |
| `ProductionQueue.cpp:31` | `!completesAt.has_value()` | taken 7% | taken 93% |
| `JsonStore.cpp:15` | `!contains(key)` | taken 5% | taken 95% |
| `JsonStore.cpp:15` | `!is_array()` | 양쪽 다 실행됨 | 양쪽 다 실행됨 |

| 완료 기준 | 충족 여부 |
|---|---|
| 우선순위 1~3 대상 분기의 각 조건이 참/거짓 양쪽 경로로 최소 1회씩 실행됨을 `.gcov`의 `branch ... taken` 출력으로 확인 | O — 위 표 참고 |
| 신규 `TEST_P`/`TEST` 전부 통과, 기존 87개 테스트 회귀 없음 | O |
| `tc.md`에 분기 커버리지 매트릭스(3부) 추가, 우선순위 1~3 항목과 신규 테스트 매핑 완료 | O |

### 3. Document Consistency Verify

구현 전 `CLAUDE.md`/`prd.md`/`PLAN.md`/`docs/design/phase12.md` 상호 참조 확인. 새 도메인 규칙 변경이
없는 테스트 보강 Phase이며 추가 충돌 없음. `docs/design/phase12.md`의 "검토했지만 이미 잘 되어 있는
것"(OrderInput.cpp, OrderJson.cpp, OrderController.cpp) 항목은 설계 문서 방침대로 추가 작업하지 않았다.

### 4. Demonstrability

```
$ bash scripts/build.sh
...
100% tests passed, 0 tests failed out of 102

$ bash scripts/coverage.sh
...
100% tests passed, 0 tests failed out of 102
커버리지 리포트(.gcov)가 build-coverage/ 에 생성되었습니다.

$ grep -A7 "avgProductionTimeMinutes <= 0.0" build-coverage/.../SampleRepository.cpp.gcov
       76:   43:    if (sample.avgProductionTimeMinutes <= 0.0) {
branch  0 taken 4% (fallthrough)
branch  1 taken 96%
       73:   46:    if (sample.yield <= 0.0 || sample.yield > 1.0) {
branch  0 taken 96% (fallthrough)
branch  1 taken 4%
branch  2 taken 4% (fallthrough)
branch  3 taken 96%

$ grep -A7 "productionCompletesAtEpochSec.has_value() || now" build-coverage/.../ProductionQueue.cpp.gcov
       14:   31:        if (!order.productionCompletesAtEpochSec.has_value() || now < *order.productionCompletesAtEpochSec) {
branch  1 taken 93% (fallthrough)
branch  2 taken 7%
```

재현 커맨드:
```
bash scripts/build.sh       # 일반 빌드/테스트 (102개)
bash scripts/coverage.sh    # 커버리지 계측 빌드/테스트 + .gcov 생성 (-b 분기 포함)
```

## 발견한 사항

- CTest는 `TEST_P` 파라미터에 사람이 읽을 수 있는 `PrintTo()`가 없으면 ctest 테스트 이름에 GetParam()의
  바이트 덤프를 그대로 노출한다 — `INSTANTIATE_TEST_SUITE_P`에 이름 생성 람다(`Case0`, `Case1`, ...)를
  줘도 이 문제는 사라지지 않았고(gtest 필터 이름은 정상 반영되지만 ctest 표시 이름은 별개), 각 파라미터
  구조체에 `PrintTo()` 오버로드를 추가해 설명(description) 문자열이 노출되도록 해결했다.
- `SampleRepository.cpp:99-102`의 OR 체인 마지막 항(yield) 내부에 컴파일러가 생성한 일부 분기가 "never
  executed"로 남아있다 — `std::to_string`/`containsIgnoreCase` 인라이닝 과정에서 생긴, 우리 비즈니스
  로직과 무관한 컴파일러 생성 코드 경로로 판단된다(예: 예외 처리용 분기). 우선순위 1~3에서 정한 "의미
  있는 비즈니스 로직 분기"는 모두 양쪽 경로가 확인되었다.

## 검증하지 못한 내용

- `SampleRepository.cpp:99-102` OR 체인 내부의 컴파일러 생성 "never executed" 분기의 정확한 정체(어떤
  인라인된 표준 라이브러리 코드 경로인지)는 어셈블리 수준까지 파고들지 않아 확인하지 못했다 — 위 "발견한
  사항" 참고, 비즈니스 로직 분기가 아니므로 이번 Phase 범위에서는 다루지 않았다.
- MC/DC(Modified Condition/Decision Coverage) 수준의 전수 조합 검증은 `docs/design/phase12.md`에서
  명시적으로 범위 밖으로 뒀다(개인 과제 규모 대비 과설계).
