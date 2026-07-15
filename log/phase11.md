# Phase 11 구현 완료 및 Verify 결과

새 기능·동작 변경 없이 테스트만 추가하는 Phase다. 이 프로젝트의 마지막 Phase다.

- 관련 설계 문서: `docs/design/phase11.md`
- 관련 계획: `PLAN.md` Phase 11

## 구현 내용

| 파일 | 내용 |
|---|---|
| `tests/ConsoleViewTest.cpp` | 신규. `mvc` PoC의 `CaptureStdout`/`GetCapturedStdout` 패턴 이식. `ConsoleView`의 `IView` 메서드 10개 전부(빈 목록 케이스 포함) 15개 테스트로 검증 |
| `tests/SampleRepositoryTest.cpp` | `SearchMatchesByProductionTimeAndYield` 추가 — `avgProductionTimeMinutes`/`yield` 수치형 필드로도 검색 매치되는지, 교차 오탐은 없는지 확인 |
| `tests/OrderControllerTest.cpp` | `ApproveOrderWithInsufficientStockShowsProductionWaitingMessage` 추가 — 재고 부족(PRODUCING) 승인 시 "생산 대기" 문구가 포함된 메시지가 전달되는지 확인 |
| `tests/CMakeLists.txt` | `ConsoleViewTest.cpp` 빌드 대상 추가 |
| `tc.md` | 1부 인벤토리에 신규 테스트 17개 반영, 통계 갱신 (Phase 1~8 70개 → Phase 1~11 87개, Phase 9·10은 테스트 수 변화 없음을 명시) |
| `CMakeLists.txt` | `ENABLE_COVERAGE` 옵션 추가 (`--coverage -O0 -g` 계측, 기본 OFF) |
| `scripts/coverage.sh` | 신규. `build-coverage/`에서 계측 빌드 → `ctest` → `gcov` 실행까지 자동화 |
| `.gitignore` | `build-coverage/` 추가 |

## Verify 결과

### 1. Test Verify — `scripts/build.sh`

```
$ bash scripts/build.sh
...
100% tests passed, 0 tests failed out of 87
```

결과: **통과 (87/87, 신규 17개 포함, Phase 1~10의 기존 70개 회귀 없음)**. 이번 Phase도 Phase 8/9/10과
마찬가지로 구현 중 테스트 실패 없이 첫 시도에 통과했다. 임의 완화/우회 없음.

### 2. Compliance Verify — `docs/design/phase11.md` / `PLAN.md` Phase 11 요구사항 대조 (커버리지 전/후 비교 포함)

`scripts/coverage.sh`를 두 번 실행해 비교했다: (A) 이번 Phase의 테스트 3종을 임시로 제거한 "보강 전"
상태(`git stash`로 일시 되돌림), (B) 복원한 "보강 후"(현재) 상태.

| 파일 | 보강 전 | 보강 후 | 비고 |
|---|---|---|---|
| `src/view/ConsoleView.cpp` | **0%** (`.gcda` 자체가 생성되지 않음 — 실행된 적 없음) | **100%** (66줄 전부) | `ConsoleViewTest.cpp` 신설 효과 |
| `src/controller/OrderController.cpp` | 94.59% (37줄 중) | **97.30%** | PRODUCING 분기 메시지 라인이 새로 커버됨 |
| `src/model/SampleRepository.cpp` | 95.83% (72줄 중) | 95.83% (**불변**) | 아래 "발견한 사항" 참고 |

| 요구사항 | 충족 여부 |
|---|---|
| 신규 테스트 전부 통과 | O |
| 기존 70개 테스트 전체 회귀 없음 | O |
| `tc.md` 1부 인벤토리 갱신 | O |
| `scripts/coverage.sh` 실행 성공, `.gcov` 리포트 생성 | O |
| 보강 전/후 비교에서 3개 영역의 라인 커버리지가 실제로 상승 | **부분 충족** — ConsoleView/OrderController는 상승, SampleRepository는 불변 (원인 아래 기록) |

### 3. Document Consistency Verify

구현 전 `CLAUDE.md`/`prd.md`/`PLAN.md`/`docs/design/phase11.md` 상호 참조 확인. 새 도메인 규칙 변경이
없는 테스트 보강 Phase이며 추가 충돌 없음.

### 4. Demonstrability — `scripts/coverage.sh` 실행 로그

```
$ bash scripts/coverage.sh
...
100% tests passed, 0 tests failed out of 87
커버리지 리포트(.gcov)가 build-coverage/ 에 생성되었습니다.

$ grep -A1 "ConsoleView.cpp'" build-coverage/**/*.gcov 관련 stdout
File 'C:/Users/User/semi/src/view/ConsoleView.cpp'
Lines executed:100.00% of 66
```

재현 커맨드:
```
bash scripts/build.sh       # 일반 빌드/테스트
bash scripts/coverage.sh    # 커버리지 계측 빌드/테스트 + .gcov 생성
```

## 발견한 사항: `SampleRepository.cpp` 라인 커버리지가 그대로인 이유

`SearchMatchesByProductionTimeAndYield`는 `search()` 내부의 `matched = containsIgnoreCase(id, kw) ||
containsIgnoreCase(name, kw) || containsIgnoreCase(avgTime, kw) || containsIgnoreCase(yield, kw)` 한
줄을 다른 **입력값**(수치형 필드가 매치되는 keyword)으로 다시 실행한다. gcov의 **라인 커버리지**는 이
한 줄이 "실행되었는가"만 세므로, 기존 `SearchMatchesPartialCaseInsensitive`가 이미 이 줄을 최소 1회
실행시킨 시점에 100%로 카운트되어 있었다 — 새 테스트가 추가한 것은 "이 줄 안의 4개 OR 항 중 뒤 두 항이
실제로 true를 내는 경로"인데, 이는 **분기(branch)/조건(condition) 커버리지**의 영역이라 이번 Phase에서
쓴 `gcov -p`(라인 단위) 리포트로는 차이가 드러나지 않는다.

즉, 새 테스트는 실제로 이전에는 검증되지 않았던 "수치형 필드로 검색이 매치되는 경로"를 최초로 실행·확인
했다는 점에서 여전히 가치가 있다 — 다만 이번 Phase의 완료 기준으로 잡은 "라인 커버리지 상승"이라는
지표로는 그 가치가 보이지 않는다는 것을 있는 그대로 기록한다. `gcov --branch-probabilities`(`-b`
옵션)를 쓰면 분기 커버리지까지 볼 수 있으나, 이번 Phase 범위(`gcovr`/`lcov` 등 추가 도구 도입 없이 원본
`gcov`만 사용, 사람 확인 완료)에서는 다루지 않았다.

## 검증하지 못한 내용

- `SampleRepository.cpp`의 **분기 커버리지**(어떤 OR 조건 항이 실제로 매치를 발생시켰는지)는 확인하지
  않았다 — 위 "발견한 사항" 참고.
- `MainController`의 `std::cin` 입력 루프 자체에 대한 자동화 테스트는 여전히 미검증 범위로 유지 (Phase
  2부터 일관).
- 코드 커버리지 100%가 "버그 없음"을 의미하지 않는다는 통상적 한계는 이번 Phase에도 동일하게 적용된다 —
  `ConsoleView.cpp`가 100%가 되었다고 해서 모든 포맷 문자열의 모든 입력 조합이 검증된 것은 아니다(예:
  긴 한글 문자열의 폭 정렬 등은 다루지 않음).

## 프로젝트 완료 메모

PLAN.md에 정의된 Phase 1~11(기능 Phase 1~8 + 클린업 Phase 9 + 버그 수정 Phase 10 + 테스트 보강 Phase
11) 전체가 완료되었다. 최종 테스트 현황은 87/87 통과.
