# Phase 12 설계: 분기/조건 커버리지 강화 (다양한 입력 기반 TC 확대)

`PLAN.md`의 Phase 12 목표(분기 계측 도입 + 다중 조건 분기의 조건별 커버리지 보강)에 대한 상세 설계.
Phase 11에서 발견된 "라인 커버리지로는 다중 조건 분기의 개별 조건 커버리지가 드러나지 않는다"는 한계를
이어받아, 소스 전체를 전수 조사하고 체계적으로 보강한다.

## 배경: 라인 커버리지 vs 조건(분기) 커버리지

`log/phase11.md`에서 실제로 겪은 사례: `SampleRepository::search()`의

```cpp
const bool matched = containsIgnoreCase(s.id, keyword) ||
                      containsIgnoreCase(s.name, keyword) ||
                      containsIgnoreCase(std::to_string(s.avgProductionTimeMinutes), keyword) ||
                      containsIgnoreCase(std::to_string(s.yield), keyword);
```

이 한 줄은 기존 테스트(`SearchMatchesPartialCaseInsensitive`)가 이미 1회 실행시켜 **라인 커버리지는
Phase 2부터 계속 100%**였다. Phase 11에서 `avgProductionTimeMinutes`/`yield`로 검색하는 케이스를
추가했지만, "이 줄이 실행됐는가"라는 지표로는 차이가 보이지 않았다 — 필요한 건 "OR의 3번째/4번째 항이
**실제로 매치를 결정한 경로**를 탔는가"라는 **조건(condition) 커버리지**다.

이번 Phase는 이 한 사례에서 그치지 않고, **소스 전체의 `||`/`&&` 표현식을 전수 조사**해 같은 문제가
있는 곳을 찾아 체계적으로 보강한다.

## 1. `gcov` 분기 계측 활성화

```bash
# scripts/coverage.sh — gcov 호출에 -b(분기 확률) 추가
gcov -b -p -o "$dir" "$dir"/*.gcda >/dev/null
```

`-b`를 켜면 `.gcov` 파일에 `branch N taken X%` 형태로 각 분기의 실행 여부가 함께 출력된다. 이번
Phase의 "완료 기준" 확인에 이 출력을 직접 근거로 쓴다 (`gcovr`/`lcov` 등 추가 도구는 Phase 11과
동일하게 도입하지 않는다).

## 2. 분기 인벤토리 (소스 전수 조사 결과)

`src/` 전체에서 `||`/`&&`가 쓰인 지점을 모두 찾았다. 아래는 우선순위별 정리다.

### 우선순위 1 — 이미 알려진 대상

| 위치 | 조건식 | 현재 상태 |
|---|---|---|
| `SampleRepository.cpp:99-101` | `id 매치 \|\| name 매치 \|\| avgTime 매치 \|\| yield 매치` | 4항 모두 "매치시키는 입력"은 있으나(Phase 11), 각 항이 **다른 항은 거짓인 상태에서** 매치를 결정하는지 독립적으로 확인한 적은 없음 |

### 우선순위 2 — 검증 체인 (경계값 미세분화 필요)

| 위치 | 조건식 | 현재 상태 |
|---|---|---|
| `SampleRepository.cpp:46` | `yield <= 0.0 \|\| yield > 1.0` | `RejectsRangeViolationsFromDummyInvalidSet`가 "위반이 하나 이상 있다"만 확인(`rangeViolationCases > 0`) — `yield<=0`과 `yield>1.0`을 **각각** 원인으로 하는 케이스가 실제로 생성·실행됐는지는 구분하지 않음 |
| `SampleRepository.cpp:43` (단일 조건이지만 연쇄 검증의 일부) | `avgProductionTimeMinutes <= 0.0` | 동일하게 dummygen 무작위 생성에 의존, 경계값(정확히 0, 음수)이 각각 시도됐는지 불명 |

### 우선순위 3 — 방어적 분기 (손상/레거시 데이터 대비)

| 위치 | 조건식 | 현재 상태 |
|---|---|---|
| `ProductionQueue.cpp:31` | `!completesAt.has_value() \|\| now < *completesAt` | `!completesAt.has_value()`는 "생산 시작은 됐는데 완료시각이 없는" 손상된 데이터를 방어하는 항 — 정상 흐름에서는 시작 시 항상 둘 다 같이 저장되므로 **현재 테스트로는 이 항이 참이 되는 경로가 없음** |
| `src/persistence/JsonStore.cpp:15` | `!root.contains(key) \|\| !root[key].is_array()` | `samples`/`orders` 키가 아예 없는 경우(현재 `BootstrapsWhenFileMissing`이 다루는 "파일 자체가 없음"과는 다름 — "파일은 있는데 키가 없거나 타입이 잘못된" 경우)는 테스트가 없음 |

### 검토했지만 이미 잘 되어 있는 것 (참고용)

| 위치 | 조건식 | 확인 결과 |
|---|---|---|
| `OrderInput.cpp:11` | `pos != raw.size() \|\| value <= 0` | `RejectsZero`/`RejectsNegative`(트레일링 없음, 값만 위반) vs `RejectsTrailingGarbage`(`"5abc"`, 값은 양수·트레일링만 위반)로 **이미 두 항이 독립적으로 분리되어 있음** — 이번 Phase에서 추가 작업 불필요, 다른 항목의 모범 사례로 참고 |
| `OrderJson.cpp:31,34` | `contains(...) && !is_null()` | "필드 자체 없음"(레거시 데이터 호환 테스트)과 "필드는 있지만 값이 채워짐"(생산 시작된 주문의 정상 라운드트립)이 기존 테스트들에 걸쳐 이미 자연스럽게 분산되어 있음 — 별도 보강 불필요 |
| `OrderController.cpp:8,21` | `result.success && result.order.has_value()` | 현재 구현상 `success`가 참이면 `order`도 항상 값을 가지므로 두 항이 독립적으로 변하지 않는 방어적 코드 — 실질적 분기가 아니므로 보강 대상에서 제외 |

## 3. 보강 방식: 파라미터라이즈드 테스트(`TEST_P`) 우선

우선순위 1·2 항목처럼 "같은 로직을 여러 입력 조합으로 반복 검증"하는 경우, `TEST()`를 조건 수만큼
복붙하지 않고 `gtest`의 `TEST_P`/`INSTANTIATE_TEST_SUITE_P`로 표 기반 케이스를 나열한다 — 이 프로젝트가
지금까지 지켜온 DRY 원칙(Phase 7의 `submenu::run()` 리팩토링 등)과 방향을 맞춘다.

```cpp
// tests/SampleRepositoryTest.cpp에 추가할 형태 (예시)
struct SearchFieldCase { std::string description; std::string keyword; bool expectMatch; };

class SampleSearchFieldTest : public ::testing::TestWithParam<SearchFieldCase> {};

TEST_P(SampleSearchFieldTest, MatchesExpectedField) {
    // 고정된 시료 1건(id="S-001", name="Silicon", avgProductionTimeMinutes=1.25, yield=0.75)에 대해
    // GetParam().keyword로 검색했을 때 GetParam().expectMatch와 일치하는지 확인
}

INSTANTIATE_TEST_SUITE_P(EachFieldIndependently, SampleSearchFieldTest, ::testing::Values(
    SearchFieldCase{"id만 매치",       "s-001", true},
    SearchFieldCase{"name만 매치",     "silic", true},
    SearchFieldCase{"avgTime만 매치",  "1.25",  true},
    SearchFieldCase{"yield만 매치",    "0.75",  true},
    SearchFieldCase{"어느 것도 매치 안 함", "zzz", false}
));
```

`registerSample()`의 `yield`/`avgProductionTimeMinutes` 검증도 동일한 패턴으로, dummygen 무작위 생성에
기대지 않고 **의도적으로 고른 경계값**(정확히 0, 음수, 정확히 1.0(유효), 1.0 초과)을 표로 나열한다.

## 4. 방어적 분기(우선순위 3)는 손상 데이터 픽스처로

`DummyGenerator`의 스키마 기반 생성은 `Sample` 도메인 검증용이라 이 두 항목(원본 JSON 구조 손상)에는
맞지 않는다 — `json_crud`/`DummyDataGenerator-djdj07` PoC의 "edge" 모드(의도적으로 깨진 JSON) 철학을
참고해, 아래처럼 **직접 작성한 손상 JSON 픽스처**로 검증한다.

```cpp
// tests/ProductionQueueTest.cpp에 추가할 형태 (예시)
TEST(ProductionQueueTest, SkipsOrderMissingCompletionTimeDespiteHavingStarted) {
    // productionStartedAtEpochSec는 있지만 productionCompletesAtEpochSec가 없는(수동으로 깨진)
    // 주문 JSON을 직접 store에 심어둔 뒤 advance()가 예외 없이 건너뛰는지 확인
}

// tests/JsonStoreTest.cpp에 추가할 형태 (예시)
TEST(JsonStoreTest, RepairsMissingSamplesOrOrdersKeyInExistingFile) {
    // {"orders": []}처럼 samples 키가 아예 없는 파일을 미리 써 둔 뒤 ensureLoaded()가
    // samples를 빈 배열로 보정하는지 확인 (파일 자체가 없는 BootstrapsWhenFileMissing과는 다른 케이스)
}
```

## 디렉토리/파일 변경 요약

| 파일 | 변경 |
|---|---|
| `scripts/coverage.sh` | `gcov` 호출에 `-b` 추가 |
| `tests/SampleRepositoryTest.cpp` | `SampleSearchFieldTest`(`TEST_P`, 5케이스), 등록 검증 경계값 `TEST_P` 추가 |
| `tests/ProductionQueueTest.cpp` | `SkipsOrderMissingCompletionTimeDespiteHavingStarted` 추가 |
| `tests/JsonStoreTest.cpp` | `RepairsMissingSamplesOrOrdersKeyInExistingFile` 추가 |
| `tc.md` | 3부 "분기/조건 커버리지 매트릭스" 신설 — 우선순위 1~3 표와 대응 테스트 매핑 |

## Phase 12에서 하지 않는 것

- 완전한 MC/DC(Modified Condition/Decision Coverage) 수준의 전수 조합 테스트 — 개인 과제 규모에 비해
  과설계이므로, 위 우선순위 1~3처럼 **의미 있는 비즈니스 로직 분기**로 범위를 한정한다
  ("검토했지만 이미 잘 되어 있는 것"에 정리된 항목은 추가 작업 없이 현행 유지)
- `gcovr`/`lcov` 등 HTML 리포트 도구 도입 (Phase 11에서 이미 결정)
- `MainController`의 `std::cin` 입력 루프, `SubMenu::run()` 자체에 대한 테스트 (계속 별도 범위 유지)

## 완료 기준

- `scripts/coverage.sh` 실행 결과(`-b` 포함)에서 우선순위 1~3 대상 분기의 각 조건이 참/거짓 양쪽 경로로 최소 1회씩 실행됨을 `.gcov`의 `branch ... taken` 출력으로 확인
- 신규 `TEST_P`/`TEST` 전부 통과, 기존 87개 테스트 회귀 없음
- `tc.md`에 분기 커버리지 매트릭스(3부) 추가, 우선순위 1~3 항목과 신규 테스트 매핑 완료
