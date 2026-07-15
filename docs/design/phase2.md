# Phase 2 설계: 시료(Sample) 모델 및 CRUD

`PLAN.md`의 Phase 2 목표(시료 등록/조회/검색 기능 완성)에 대한 상세 설계. Phase 1 코드 리뷰에서 발견되어
`PLAN.md`에 반영된 3가지 확정 필요 사항(공유 저장소, 부트스트랩, Controller 분리 방향)을 이번 Phase에서 해소한다.

## 확정된 결정 사항

### 1. 공유 저장소: `JsonStore` 신설

`SampleRepository`(Phase 2)와 `OrderRepository`(Phase 3)가 동일한 `data.json`을 다뤄야 하므로, 각자 독립적인
`JsonDocument`를 갖지 않고 **하나의 `JsonStore` 인스턴스를 공유 참조**한다.

```cpp
// src/persistence/JsonStore.h
namespace sampleorder {

class JsonStore {
public:
    explicit JsonStore(std::filesystem::path file);

    // 파일이 없으면 {"samples":[], "orders":[]} 로 생성 후 로드, 있으면 그대로 로드
    void ensureLoaded();

    nlohmann::ordered_json& samples();   // root_["samples"] 참조 (Phase 2에서 사용)
    nlohmann::ordered_json& orders();    // root_["orders"] 참조 (Phase 3에서 사용, 지금은 빈 배열 유지만)

    void save();                        // root_ 전체를 파일에 저장 (JsonDocument::save 위임)

private:
    JsonDocument doc_;
    std::filesystem::path file_;
};

} // namespace sampleorder
```

- `SampleRepository`는 `JsonStore&`를 생성자로 주입받아 `samples()`를 읽고 수정한 뒤 `save()`를 호출한다.
- 매 CRUD 연산 직후 `save()`를 호출한다(연산 단위 즉시 저장 — 배치 저장은 하지 않음). 개인 과제 규모(콘솔 단일 사용자, 동시 프로세스 없음)에서는 매 연산 저장이 가장 단순하고 안전하다.
- Phase 3에서 `OrderRepository`도 동일한 `JsonStore` 인스턴스를 주입받아 `orders()`를 사용하게 되며, 이 시점에 별도 리팩토링이 필요 없다.

### 2. 최초 실행 부트스트랩

`JsonStore::ensureLoaded()`가 `std::filesystem::exists(file_)`로 파일 존재를 확인하고:
- 없으면: `root_ = {"samples": nlohmann::ordered_json::array(), "orders": nlohmann::ordered_json::array()}` 로 초기화 후 즉시 `save()`
- 있으면: 기존 `JsonDocument::load()` 그대로 사용

`main.cpp`에서 앱 시작 시 1회 `store.ensureLoaded()`를 호출한다.

### 3. Controller 책임 분리: 도메인별 Controller 도입

`MainController`는 계속 확장하지 않고, 이번 Phase부터 **도메인별 Controller를 신설**한다.

- `SampleController` 신설 — 시료 등록/조회/검색 흐름 전담
- `MainController`는 메뉴 "1" 선택 시 `SampleController`로 위임만 하고, 나머지("2"~"6")는 Phase 1과 동일하게 "미구현" 유지
- 이후 Phase 3~7에서도 `OrderController`, `ProductionController`, `MonitoringController` 등을 같은 방식으로 추가해 나간다 (본 Phase에서 규칙만 확정, 실제 신설은 각 Phase에서)

## 도메인 모델

`prd.md` §3.1 기준. 시료 등록 입력값에 재고는 없으므로 **등록 시 재고는 항상 0으로 시작**하고, 이후 재고는 오직
Phase 5(생산 완료)를 통해서만 증가한다 — 이번 Phase에서 재고를 직접 늘리는 기능은 만들지 않는다.

```cpp
// src/model/Sample.h
struct Sample {
    std::string id;                 // 사용자가 직접 입력 (예: "S-001"), 자동 생성 아님
    std::string name;
    double avgProductionTimeMinutes;
    double yield;                   // (0, 1] 범위
    int stock = 0;                  // 등록 시 항상 0, Phase 5에서만 증가
};
```

```cpp
// src/model/SampleRepository.h
class SampleRepository {
public:
    explicit SampleRepository(JsonStore& store);

    // 실패 사유: 이미 존재하는 id, avgProductionTime <= 0, yield가 (0,1] 밖
    // → 성공/실패와 사유를 함께 반환 (mvc의 optional 리턴 패턴 확장)
    struct RegisterResult { bool success; std::string errorMessage; };
    RegisterResult registerSample(const Sample& sample);

    std::optional<Sample> find(const std::string& id) const;
    std::vector<Sample> list() const;
    // id/name/avgProductionTimeMinutes/yield 각 필드를 문자열로 변환해 keyword가 부분 포함(대소문자 무시)되면 매치.
    // 하나라도 일치하는 필드가 있으면 결과에 포함 (OR 매칭).
    std::vector<Sample> search(const std::string& keyword) const;

private:
    JsonStore& store_;
};
```

- mvc PoC의 `ItemRepository`(벡터 CRUD)와 달리 실제 CRUD 대상이 `JsonStore` 기반이므로, 매 호출마다 `store_.samples()`를 순회/수정한다 (메모리 캐시 별도로 두지 않음 — 데이터 규모가 작은 개인 과제이므로 단순성 우선).
- 검색 규칙(사람 확인 완료): id/name/avgProductionTimeMinutes/yield 전체 필드 대상, 부분 일치 + 대소문자 무시, 하나라도 매치하면 결과에 포함(OR).

## View / Controller

```cpp
// src/view/IView.h — Sample 표시 메서드 추가 (기존 showMessage/showError 유지)
virtual void showSamples(const std::vector<Sample>& samples) = 0;
virtual void showSample(const Sample& sample) = 0;
```

```cpp
// src/controller/SampleController.h
class SampleController {
public:
    SampleController(SampleRepository& repository, IView& view);

    void registerSample(const std::string& id, const std::string& name,
                         double avgProductionTimeMinutes, double yield);
    void listSamples();
    void searchSamples(const std::string& keyword);
};
```

`MainController::handleSelection("1")`은 시료 관리 하위 메뉴(등록/조회/검색/뒤로가기)로 진입하는 루프를 시작하고,
사용자 입력을 받아 `SampleController`의 메서드를 호출한다 (mvc PoC의 `main.cpp` 입력 처리 방식을 그대로 하위 메뉴에 적용).

## 디렉토리/파일 변경 요약

| 파일 | 변경 |
|---|---|
| `src/persistence/JsonStore.h/.cpp` | 신규 |
| `src/model/Sample.h` | 신규 |
| `src/model/SampleRepository.h/.cpp` | 신규 |
| `src/view/IView.h` | `showSamples`/`showSample` 추가 |
| `src/view/ConsoleView.h/.cpp` | 위 메서드 구현 추가 |
| `src/controller/SampleController.h/.cpp` | 신규 |
| `src/controller/MainController.h/.cpp` | "1" 선택 시 시료 관리 하위 메뉴로 진입하도록 수정 |
| `src/main.cpp` | `JsonStore`/`SampleRepository`/`SampleController` 생성 및 연결 추가 |
| `tests/JsonStoreTest.cpp` | 신규 — 부트스트랩(파일 없음/있음), 저장 후 재로드 라운드트립 |
| `tests/SampleRepositoryTest.cpp` | 신규 — 등록/중복 id 거부/검색/영속화 확인 (임시 파일 경로 사용, 실제 `data/` 미사용) |
| `tests/SampleControllerTest.cpp` | 신규 — View는 mvc 패턴대로 stub 처리 |

## Phase 2에서 하지 않는 것

- `Order` 엔티티/`OrderRepository` (Phase 3) — 단, `JsonStore::orders()`가 빈 배열을 유지하도록만 부트스트랩에서 보장
- 재고 증감 로직 (Phase 4 승인/거절, Phase 5 생산완료에서만 다룸)
- `OrderController`/`ProductionController` 등 타 도메인 Controller (해당 Phase에서 신설)

## 완료 기준

- 시료 등록 → 앱 재시작 → 조회 시 방금 등록한 시료가 그대로 조회됨 (영속화 확인, 실제 `data/data.json` 파일로 재현)
- 중복 ID 등록 시 거부되는지, 잘못된 수율/생산시간 입력 시 거부되는지 테스트로 검증
- id/name/생산시간/수율 각 필드 기준 부분 일치·대소문자 무시 검색 기능 테스트 통과 (필드별 매치 케이스 + 미매치 케이스)
- `data.json` 부재 상태에서 최초 실행 시 예외 없이 부트스트랩되는지 테스트로 검증
- `ctest` 전체(placeholder + 신규 테스트) 통과
- 콘솔 실행 로그로 등록→조회→검색 흐름 재현 (Demonstrability)

## 실제 구현 결과 (설계 대비 변경/추가 사항)

상세 Verify 결과는 `log/phase2.md` 참고. 설계 대비 다음 두 가지가 추가/변경되었다.

### 1. 테스트 데이터: DummyDataGenerator-djdj07 재사용 적용

애초 설계에는 없었으나, PLAN.md의 PoC↔Phase 매핑표("DummyDataGenerator-djdj07 → 각 Phase의 테스트 데이터
준비 시")를 지금까지 적용하지 않고 있었음을 사람이 지적하여 Phase 2부터 반영했다.

- `DummyDataGenerator-djdj07`의 `DummyGenerator.h/.cpp`, `SchemaValidator.h/.cpp`를 그대로
  `tests/dummygen/`으로 이식 (프로덕션 코드가 아닌 테스트 전용이므로 `src/`가 아닌 `tests/`에 배치).
- `tests/SampleTestData.h`에 Sample 도메인 규칙(avgProductionTimeMinutes>0, yield∈(0,1])을 반영한
  JSON 스키마(`sampleSchema()`)와 JSON↔`Sample` 변환 헬퍼(`toSample()`)를 신설.
- `SampleRepositoryTest`/`SampleControllerTest`는 하드코딩 대신
  `DummyGenerator::generateValidFromSchema`(정상 케이스)와 `generateInvalidFromSchema`(범위 위반
  케이스, `avgProductionTimeMinutes`/`yield`의 최솟값·최댓값 위반만 도메인 규칙 위반으로 필터링해 검증)로
  생성한 데이터를 사용하도록 재작성.
- `generateValidFromSchema`는 완전히 결정론적(같은 스키마 → 항상 같은 값)이라, 여러 건이 필요한
  테스트(목록/검색)는 생성된 기본값에서 `id`/`name`만 덮어써 구분한다.

### 2. CMake 구조 변경: `sample_order_core` 라이브러리 신설

Phase 1에서는 `sample_order_app`이 `main.cpp`/`ConsoleView.cpp`/`MainController.cpp`를 직접
컴파일했다. Phase 2부터 테스트 대상 코드(모델/뷰/컨트롤러)가 늘어나면서, 테스트 실행 파일이 동일 소스를
다시 컴파일하지 않고 공유할 수 있도록 `sample_order_core` 정적 라이브러리를 신설했다.

- `sample_order_persistence`: `JsonDocument`, `JsonStore` (기존과 동일)
- `sample_order_core`: `SampleRepository`, `ConsoleView`, `SampleController`, `MainController` (신설, `sample_order_persistence`에 링크)
- `sample_order_app`: `main.cpp`만 컴파일, `sample_order_core`에 링크
- `sample_order_tests`: `sample_order_core` + `tests/dummygen` 소스 + gtest에 링크

### 3. 빌드 환경 이슈 발견 및 해결: Git Bash mingw64 PATH 충돌

`ctest` 실행 시 신규 테스트 실행 파일이 `0xc0000139`(`STATUS_ENTRYPOINT_NOT_FOUND`)로 즉시 종료되는
문제를 발견했다. 원인은 Git Bash(MSYS)에 내장된 mingw64(`/mingw64/bin`)가 실제 빌드에 쓰인 WinGet
mingw64보다 PATH 앞쪽에 있어, 실행 파일이 컴파일에 쓰인 것과 다른(ABI 불일치) `libstdc++-6.dll`을
로드했기 때문이다(코드 결함 아님). PowerShell에서 동일 실행 파일을 직접 실행하면 문제없이 통과하는 것으로
원인을 확정했다.

해결책으로 `scripts/build.sh`를 신설해 WinGet mingw64/bin을 PATH 맨 앞에 두고 configure→build→ctest를
수행하도록 했다. 이후 모든 Phase의 빌드/테스트는 이 스크립트를 통해 실행한다 (`PLAN.md`, `CLAUDE.md`에도
반영).
