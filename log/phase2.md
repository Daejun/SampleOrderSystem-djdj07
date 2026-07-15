# Phase 2 구현 완료 및 Verify 결과

- 관련 설계 문서: `docs/design/phase2.md` (실제 구현 결과 절 포함)
- 관련 계획: `PLAN.md` Phase 2

## 구현 내용

| 파일 | 내용 |
|---|---|
| `src/persistence/JsonStore.h/.cpp` | 신규. `SampleRepository`/`OrderRepository`(Phase 3)가 공유할 저장소. `ensureLoaded()`가 파일 부재 시 `{"samples":[],"orders":[]}`로 자동 부트스트랩 |
| `src/model/Sample.h` | 신규. `id/name/avgProductionTimeMinutes/yield/stock` |
| `src/model/SampleRepository.h/.cpp` | 신규. 등록(중복 id·범위 검증)/조회/검색(부분 일치·대소문자 무시) |
| `src/view/IView.h`, `ConsoleView.h/.cpp` | `showSamples`/`showSample` 추가 |
| `src/controller/SampleController.h/.cpp` | 신규. 시료 등록/조회/검색 흐름 전담 |
| `src/controller/MainController.h/.cpp` | "1" 선택 시 시료 관리 하위 메뉴로 위임하도록 수정 |
| `src/main.cpp` | `JsonStore`/`SampleRepository`/`SampleController` 생성·연결 |
| `src/CMakeLists.txt` | `sample_order_core` 라이브러리 신설 (모델/뷰/컨트롤러 공유) |
| `tests/dummygen/DummyGenerator.h/.cpp`, `SchemaValidator.h/.cpp` | `DummyDataGenerator-djdj07` PoC에서 이식 (테스트 전용) |
| `tests/SampleTestData.h` | Sample 도메인 스키마 + JSON↔Sample 변환 헬퍼 |
| `tests/JsonStoreTest.cpp` | 신규. 부트스트랩(파일 없음/있음), 라운드트립 |
| `tests/SampleRepositoryTest.cpp` | 신규. dummygen 기반 등록/중복 거부/범위 위반 거부/검색/영속화 |
| `tests/SampleControllerTest.cpp` | 신규. dummygen 기반, `StubView`로 View 위임 확인 |
| `tests/StubView.h` | 신규. `IView` 스텁 (mvc PoC 패턴) |
| `scripts/build.sh` | 신규. PATH 충돌을 피해 configure→build→ctest를 수행하는 빌드 스크립트 |

## 사람 확인 사항 반영

- Phase 2 테스트 데이터를 `DummyDataGenerator-djdj07` 재사용으로 전환하기로 확인받음 (기존에는 하드코딩된 값 사용 중이었음 — PLAN.md의 PoC 재사용 방침과 불일치했던 부분을 사람이 지적, 수정).

## Verify 결과

### 1. Test Verify — `scripts/build.sh` (`cmake --build` / `ctest`)

빌드 중간에 `ctest`가 신규 테스트 실행 파일에서 `0xc0000139`(`STATUS_ENTRYPOINT_NOT_FOUND`)로
즉시 종료되는 문제가 있었다. 원인은 Git Bash 내장 mingw64가 WinGet mingw64보다 PATH 앞에 있어
ABI가 다른 `libstdc++-6.dll`을 로드했기 때문 (상세: 아래 "발견한 이슈" 절). `scripts/build.sh`로
PATH를 바로잡아 해결했으며, 이후 이 스크립트로만 빌드/테스트를 수행한다.

```
$ bash scripts/build.sh
...
[100%] Built target sample_order_tests
...
Test project C:/Users/User/semi/build
      Start  1: SampleControllerTest.RegisterSampleShowsSuccessMessage
 1/14 Test  #1: SampleControllerTest.RegisterSampleShowsSuccessMessage ...........   Passed
 2/14 Test  #2: SampleControllerTest.RegisterDuplicateShowsError .................   Passed
 3/14 Test  #3: SampleControllerTest.RegisterInvalidYieldShowsError ..............   Passed
 4/14 Test  #4: SampleControllerTest.ListSamplesForwardsToView ...................   Passed
 5/14 Test  #5: SampleControllerTest.SearchSamplesForwardsToView .................   Passed
 6/14 Test  #6: SampleRepositoryTest.RegisterAndFindDummySample ..................   Passed
 7/14 Test  #7: SampleRepositoryTest.RejectsDuplicateId ..........................   Passed
 8/14 Test  #8: SampleRepositoryTest.RejectsRangeViolationsFromDummyInvalidSet ...   Passed
 9/14 Test  #9: SampleRepositoryTest.SearchMatchesPartialCaseInsensitive .........   Passed
10/14 Test #10: SampleRepositoryTest.PersistsAcrossReload ........................   Passed
11/14 Test #11: JsonStoreTest.BootstrapsWhenFileMissing ..........................   Passed
12/14 Test #12: JsonStoreTest.LoadsExistingFile ..................................   Passed
13/14 Test #13: JsonStoreTest.SaveThenReloadRoundTrips ...........................   Passed
14/14 Test #14: Placeholder.BuildWorks ...........................................   Passed

100% tests passed, 0 tests failed out of 14
```

결과: **통과 (14/14)**. 임의 완화/우회 없음.

### 2. Compliance Verify — `docs/design/phase2.md` / `PLAN.md` Phase 2 요구사항 대조

| 요구사항 | 충족 여부 |
|---|---|
| `JsonStore` 공유 저장소, 최초 실행 부트스트랩 | O |
| `Sample`/`SampleRepository` 등록·조회·검색, 등록 시 재고 0 강제 | O |
| 중복 ID 거부, 생산시간·수율 범위 검증 | O |
| `SampleController` 신설, `MainController`는 위임만 | O |
| 검색 시 id/name/생산시간/수율 부분 일치·대소문자 무시 | O |
| 테스트 데이터 DummyDataGenerator-djdj07 재사용 | O (Phase 2부터 적용) |

### 3. Document Consistency Verify

구현 전 `CLAUDE.md`/`prd.md`/`PLAN.md`/`docs/design/phase2.md` 상호 참조 확인. 테스트 데이터 도구
미사용 문제를 발견해 사람에게 확인 후 `PLAN.md`/`prd.md`/`CLAUDE.md`에 일관되게 반영했다(본 로그 및
`docs/design/phase2.md` "실제 구현 결과" 절 참고).

### 4. Demonstrability — 콘솔 실행 로그

```
$ bash scripts/build.sh   # (내부에서 WinGet mingw64를 PATH에 우선 배치)

$ ./build/src/sample_order_app.exe
=== 반도체 시료 생산주문관리 시스템 ===
[1] 시료 관리
...
선택 > 1

--- 시료 관리 ---
[1] 시료 등록
[2] 시료 목록
[3] 시료 검색
[0] 뒤로가기
선택 > 1
시료 ID > S-001
이름 > 실리콘 웨이퍼
평균 생산시간(min/ea) > 0.5
수율(0~1) > 0.9
시료 등록 완료: S-001

선택 > 2
[S-001] 실리콘 웨이퍼 (생산시간 0.50 min/ea, 수율 0.90, 재고 0 ea)

선택 > 3
검색어 > 실리콘
[S-001] 실리콘 웨이퍼 (생산시간 0.50 min/ea, 수율 0.90, 재고 0 ea)
```

재시작 후 영속화 확인 (`data/data.json` 유지된 상태로 앱 재실행):

```
$ ./build/src/sample_order_app.exe   # 재시작, 등록 없이 목록만 조회
선택 > 1
선택 > 2
[S-001] 실리콘 웨이퍼 (생산시간 0.50 min/ea, 수율 0.90, 재고 0 ea)
```

`data/data.json` 내용:
```json
{
  "samples": [
    { "id": "S-001", "name": "실리콘 웨이퍼", "avgProductionTimeMinutes": 0.5, "yield": 0.9, "stock": 0 }
  ],
  "orders": []
}
```

재현 커맨드:
```
bash scripts/build.sh
./build/src/sample_order_app.exe
```

## 발견한 이슈: Git Bash mingw64 PATH 충돌 (환경 문제, 코드 결함 아님)

- **증상**: `cmake --build` 후 `ctest`가 신규 테스트 실행 파일에서 `Exit code 0xc0000139`로 즉시 종료.
- **원인**: Git Bash의 PATH에 Git 번들 mingw64(`/mingw64/bin`)가 WinGet 설치 mingw64보다 앞서 있어,
  실행 파일이 컴파일에 사용된 것과 ABI가 다른 `libstdc++-6.dll`/`libgcc_s_seh-1.dll`을 로드함
  (`STATUS_ENTRYPOINT_NOT_FOUND`).
- **진단 근거**: 동일 실행 파일을 PowerShell에서 직접 실행하면 정상 동작(전체 테스트 목록 정상 출력) →
  실행 파일 자체는 정상이며 Bash의 PATH 순서만 문제였음을 확인.
- **해결**: `scripts/build.sh`에서 WinGet mingw64/bin을 PATH 맨 앞에 두고 configure/build/ctest를
  실행하도록 고정. `CLAUDE.md`("빌드 환경 주의사항")와 `PLAN.md`(Phase 분해 원칙)에 반영해 앞으로의
  모든 Phase가 동일 문제를 겪지 않도록 함.

## 검증하지 못한 내용

- Windows 외 플랫폼 빌드는 여전히 시도하지 않음.
- `SampleController`/`MainController`의 하위 메뉴 입력 처리(`std::cin` 기반)에 대한 자동화된 컨트롤러 단위 테스트는 작성하지 않음 (콘솔 실행 로그로만 확인). 표준입력을 모킹하는 테스트는 Phase 3 이후 필요 시 검토.
