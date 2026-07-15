# PLAN: 반도체 시료 생산주문관리 시스템 (SampleOrderSystem)

이 문서는 `prd.md`의 요구사항을 Phase 단위로 분해한 개발 계획이다. 각 Phase의 상세 설계는 착수 직전에
`docs/design/phaseN.md`로 별도 작성하며, 이 문서에는 Phase별 목표·범위·완료 기준만 담는다.

## Phase 분해 원칙

- 각 Phase는 완료 시 빌드/테스트가 깨지지 않는 독립 실행 가능 상태여야 한다.
- 기 개발된 PoC(`json_crud`, `mvc`, `DummyDataGenerator-djdj07`, `datamon`)의 **코드를 semi 프로젝트로 직접 가져와 재사용**한다 (사람 확인 완료, 2026-07-15). 패턴만 참고하여 새로 작성하지 않는다 — 각 PoC의 소스를 semi 프로젝트 구조에 맞게 이식(porting)하는 것을 우선한다.
- 각 Phase 착수 전 `docs/design/phaseN.md` 작성 → 사람 검토 → 구현 → Verify(§CLAUDE.md) → 사람 리뷰 → 커밋 순서를 따른다.
- **빌드/테스트는 반드시 `scripts/build.sh`로 실행한다.** Git Bash 내장 mingw64와 WinGet mingw64의 런타임 DLL이 충돌해 `ctest`가 `0xc0000139`로 죽는 문제가 있다 (원인·해결: `CLAUDE.md`의 "빌드 환경 주의사항", `log/phase2.md` 참고).

---

## Phase 1 — 프로젝트 스캐폴딩

- **목표**: MVC 디렉토리 구조와 CMake 빌드 체계를 갖춘, 빌드/실행만 되는 빈 콘솔 앱
- **범위**:
  - `mvc` PoC의 CMake 설정·gtest 연동(FetchContent)·디렉토리 구조를 semi 프로젝트로 이식
  - `model/ controller/ view/` 디렉토리 구조 및 `IView` 등 인터페이스 코드를 `mvc` PoC에서 가져와 재사용
  - 데이터 영속성 계층은 `json_crud`의 `JsonDocument`(atomic write, 경로 기반 CRUD) 코드를 가져와 semi 도메인(Sample/Order)에 맞게 연결
  - 메인 메뉴 스텁(선택지만 출력, 기능 미구현)
- **완료 기준**: `cmake --build` 성공, `ctest` 통과(placeholder 테스트 포함), 메뉴 스텁 실행 확인

## Phase 2 — 시료(Sample) 모델 및 CRUD

- **목표**: 시료 등록/조회/검색 기능 완성
- **범위**: `Sample` 엔티티, `SampleRepository`(영속화 포함), 등록/조회/검색 Controller·View
- **설계 시 반드시 확정할 사항 (Phase 1 코드 리뷰에서 발견)**:
  - **공유 저장소 동시 접근**: `SampleRepository`와 `OrderRepository`(Phase 3)가 동일한 `data.json`을 다룬다. 각자 독립적인 `JsonDocument` 인스턴스로 로드·저장하면 한쪽의 저장이 다른 쪽의 최신 변경을 덮어써 데이터가 유실될 수 있다. 두 Repository가 **하나의 공유 `JsonDocument`(또는 이를 감싸는 `JsonStore`) 인스턴스**를 참조하도록 설계한다.
  - **최초 실행 부트스트랩**: `data/` 디렉토리는 `.gitignore` 대상이라 최초 실행 시 `data.json`이 존재하지 않는다. 현재 `JsonDocument::load()`는 파일이 없으면 예외를 던지므로, 파일 부재 시 `{"samples":[], "orders":[]}` 형태로 자동 생성하는 로직을 설계에 포함한다.
  - **Controller 책임 분리 방향**: Phase 1에서 만든 단일 `MainController`를 계속 확장할지, 도메인별(`SampleController` 등)로 분리할지 이번 Phase에서 결정한다. 결정하지 않으면 Phase 3~4를 거치며 `MainController`가 여러 도메인 로직을 떠안는 비대한 클래스가 될 위험이 있다(SRP 위반).
- **완료 기준**: 시료 등록 후 재시작해도 조회 가능(영속화 확인), 검색 기능 테스트 통과
- **구현 완료** (`log/phase2.md` 참고): `JsonStore`/`Sample`/`SampleRepository`/`SampleController` 구현, `MainController`의 "1" 선택 시 시료 관리 하위 메뉴로 위임하도록 수정. 테스트 데이터는 `DummyDataGenerator-djdj07`을 `tests/dummygen/`으로 이식해 스키마 기반 valid/invalid 인스턴스로 생성 (PoC 매핑표 방침 최초 적용).

## Phase 3 — 주문(Order) 모델 및 예약 접수

- **목표**: 주문 예약(RESERVED) 등록 기능 완성
- **범위**: `Order` 엔티티, 상태(enum: RESERVED/REJECTED/PRODUCING/CONFIRMED/RELEASE), `OrderRepository`, 주문 예약 입력 흐름
- **설계 시 반드시 확정할 사항 (Phase 2 코드 리뷰에서 발견)**:
  - **`JsonStore.cache_` 전제**: `JsonStore`는 설계 문서와 달리 내부 `cache_`가 실제 데이터를 들고 있고 `save()` 시점에 `doc_`로 복사된다(`docs/design/phase2.md` §4). `OrderRepository`도 이 구조를 전제로 `orders()`를 사용한다.
  - **입력 파싱과 I/O 분리 검토**: `MainController::runSampleMenu()`는 `std::cin`과 파싱/검증 로직이 결합되어 테스트가 불가능했다(`docs/design/phase2.md` §5). 주문 접수는 입력 항목(시료ID/고객명/수량)이 더 많으므로, 파싱/검증을 `std::cin`과 분리된 순수 함수로 추출해 테스트 가능하게 할지 이번 Phase에서 결정한다.
- **완료 기준**: 존재하는 시료 ID로만 주문 생성 가능, 주문 생성 시 상태가 RESERVED로 저장·조회됨

## Phase 4 — 주문 승인/거절 로직

- **목표**: 재고 확인 기반 자동 분기(CONFIRMED/PRODUCING) 및 거절(REJECTED) 처리
- **범위**: RESERVED 목록 표시, 승인 시 재고 충분→CONFIRMED / 재고 부족→PRODUCING 자동 분기, 거절 시 REJECTED 전환
- **재고 차감 규칙 (사람 확인 완료)**:
  - 재고 충분 시 승인 즉시 주문 수량만큼 `Sample.stock`을 차감하고 CONFIRMED로 전환한다.
  - 재고 부족 시 승인 즉시 기존 재고 전량을 0으로 차감(해당 주문에 전액 점유)하고, 부족분(`quantity - 승인 시점 재고`)을 주문에 저장한 뒤 PRODUCING으로 전환한다. 이 부족분을 Phase 5가 실 생산량 계산(`ceil(부족분/수율)`)에 사용한다.
- **설계 시 반드시 확정할 사항 (Phase 3 코드 리뷰에서 발견)**:
  - **Controller 재사용**: 새 도메인 Controller를 만들지 않고 기존 `OrderController`에 승인/거절 메서드를 추가한다 — `MainController`가 도메인마다 생성자 파라미터를 계속 늘려가는 문제(`log`/이전 리뷰에서 지적)를 막기 위함. `MainController::handleSelection("3")`은 새 생성자 파라미터 없이 기존 `orderController_`를 재사용한다.
  - **`OrderRepository` 상태 변경 메서드 신설**: Phase 3까지는 `reserve`/`find`/`list`만 있었다. 이번 Phase에서 `approve`/`reject`/`listByStatus` 등 상태 전이 메서드를 추가한다.
- **완료 기준**: 재고 충분/부족/거절 3가지 시나리오 각각 테스트로 검증, 부족 시 부족분 계산이 정확한지 검증, 이미 처리된(RESERVED가 아닌) 주문의 재승인/재거절이 거부되는지 검증

## Phase 5 — 생산 라인 (FIFO 큐)

- **목표**: 단일 생산 라인의 FIFO 생산 큐와 생산량/생산시간 계산 완성
- **범위**: 생산 큐 자료구조, 실 생산량(`ceil(부족분 / 수율)`) 및 총 생산 시간(`평균 생산시간 * 실 생산량`) 계산, 생산 완료 시 PRODUCING→CONFIRMED 자동 전환, 생산 현황/대기 목록 조회
- **설계 시 반드시 확정할 사항 (Phase 4 코드 리뷰에서 발견)**:
  - **`store_.save()` 중복 호출**: `OrderRepository::approve()`가 `SampleRepository::setStock()`(내부에서 자체 저장)을 호출한 뒤 자신도 다시 `store_.save()`를 호출해, 승인 1건당 파일 쓰기가 2번 발생한다(`docs/design/phase4.md` 코드 리뷰 지적, 정확성 문제는 아님). Phase 5의 생산 큐 진행(`advance()`)은 재고 갱신 + 주문 상태 전이를 동시에 수행하므로 같은 패턴이 반복되면 저장 횟수가 더 늘어난다. 이번 Phase에서 "저장하지 않는 내부 mutate + 호출부에서 단일 `save()`" 패턴을 도입하고, 여유가 되면 Phase 4의 `approve()`도 함께 정리한다.
- **생산 규칙 (필수 준수)**:
  - **주문 단위 생산**: 생산은 주문 1건 단위로 실행된다. 한 주문의 생산이 끝나야 다음 주문의 생산을 시작한다.
  - **Batch 재고 갱신**: 재고는 주문 단위 생산이 "완료"된 시점에 한 번에(batch) 갱신한다. 생산 중간에 부분 수량을 재고에 반영하지 않는다.
  - **생산 중 수량은 재고 아님**: PRODUCING 중인 수량은 현재 재고(가용 재고)에 포함하지 않는다. 재고 조회·모니터링·재고 충분/부족 판정 시 생산 중 수량을 제외한 실제 보유 재고만 사용한다.
  - **수율의 역할 한정**: 수율은 승인 시점에 "부족분을 메우기 위해 얼마나 생산해야 하는가(실 생산량 = `ceil(부족분 / 수율)`)"를 계산하는 데에만 관여한다. 생산이 완료되면 계산된 실 생산량 전량이 그대로 재고에 추가된다 — 완료 시점에 수율을 다시 곱하거나 추가로 감산하지 않는다.
  - **현실 시간 기반 생산**: 총 생산 시간(`평균 생산시간 * 실 생산량`)은 실제 경과 시간(wall-clock)과 동일하게 흐른다. 시뮬레이션 배속이나 가속 없이 실시간으로 진행된다.
  - **프로그램 생명주기 독립적 완료 판정**: 생산 시작 시각과 예정 완료 시각(또는 소요 시간)을 영속 저장한다. 프로그램이 생산 도중 종료되었다가 예정 완료 시각 이후에 재시작되면, 재시작 시점에 즉시 생산 완료로 판단하고 PRODUCING→CONFIRMED 전환 및 재고 batch 갱신을 수행한다. 생산 진행 여부는 프로세스 실행 여부와 무관하게 오직 저장된 시각 비교로만 결정한다.
  - **큐 병합 금지**: 생산 큐는 1개만 존재하며, 대기 중인 여러 주문의 부족분을 하나의 생산 작업으로 합치는(merge) 동작은 허용하지 않는다. 각 주문은 큐에 개별 항목으로 유지되고 FIFO 순서대로 하나씩 순차 생산된다.
- **완료 기준**:
  - FIFO 순서 보장 테스트, 계산 공식(`ceil(부족분/수율)`, `평균 생산시간*실 생산량`) 단위 테스트
  - 생산 완료 시 PRODUCING→CONFIRMED 전환 및 재고 batch 반영(부분 반영 없음) 테스트
  - 생산 중 수량이 가용 재고 조회에 포함되지 않는지 검증하는 테스트
  - 완료된 실 생산량 전량이 수율 재적용 없이 재고에 더해지는지 검증하는 테스트
  - 앱 종료 후 예정 완료 시각이 지난 뒤 재시작 시, 재시작 즉시 생산 완료 처리(전환+재고 갱신)되는지 검증하는 테스트(시각 조작/모킹 기반)
  - 큐에 여러 대기 주문이 있을 때 병합되지 않고 개별 항목으로 순차 처리되는지 검증하는 테스트

## Phase 6 — 출고 처리

- **목표**: CONFIRMED 상태 주문의 출고(RELEASE) 처리
- **범위**: CONFIRMED 주문만 출고 가능하도록 제한, 출고 실행 시 RELEASE 전환
- **완료 기준**: CONFIRMED 외 상태 출고 시도 시 거부되는지, 출고 후 RELEASE로 전환되는지 테스트 통과
- **구현 완료** (`log/phase6.md` 참고): `OrderRepository::release()` 추가(approve/reject와 동일한 단일 저장 구조), `OrderController::releaseOrder`/`listConfirmedOrders` 추가, `MainController` "6" 선택 시 출고 하위 메뉴 연결(생성자 변경 없음).

## Phase 7 — 모니터링

- **목표**: 주문량/재고량 모니터링 기능 완성
- **범위**: 상태별 주문 건수 집계(REJECTED 제외), 재고 상태 판정(여유/부족/고갈)
- **재고 판정 기준 (사람 확인 완료)**: 시료별로 `stock == 0`이면 항상 "고갈"(주문 존재 여부 무관, 최우선 판정). 그 외에는 해당 시료의 **RESERVED 주문 수량 합**과 비교해 `stock >= RESERVED 합`이면 "여유", 아니면 "부족". PRODUCING/CONFIRMED/RELEASE 주문은 이미 승인 시점에 재고 판정이 끝난 건이므로 비교 대상에서 제외한다 (PLAN.md Phase 5 "생산 중 수량은 재고 아님, 모니터링 판정 시에도 실제 보유 재고만 사용" 규칙의 연장 해석). 경계값(재고==RESERVED 합)은 Phase 4의 `ApproveExactStockMatchConfirmsWithoutShortage`와 동일하게 "여유" 쪽으로 처리한다.
- **설계 시 반드시 확정할 사항 (Phase 6 코드 리뷰에서 발견)**:
  - **서브메뉴 루프 중복 (Rule of Three)**: `MainController`의 `runSampleMenu`/`runOrderMenu`/`runApprovalMenu`/`runReleaseMenu` 4개가 거의 동일한 구조(메뉴 출력→`getline`→분기→`"0"`시 종료→그 외 오류)를 반복하고 있다(`CLAUDE.md` §4.2 Rule of Three 기준 충족). 이번 Phase에서 신설할 모니터링 서브메뉴까지 포함해 공통 헬퍼로 추출한다.
- **완료 기준**: REJECTED 제외 집계 테스트, 3구간 판정 경계값(0, 부족 임계, 여유) 테스트 통과, 서브메뉴 헬퍼 추출 후 기존 5개 메뉴 전체 회귀 없음
- **구현 완료** (`log/phase7.md` 참고): `submenu::run()` 공통 헬퍼로 기존 4개 서브메뉴 리팩터링, `MonitoringService`/`MonitoringController` 신설, `MainController` "4" 선택 시 모니터링 하위 메뉴 연결(생성자에 `MonitoringController&` 추가).

## Phase 8 — 메인 메뉴 통합 및 E2E

- **목표**: 전체 메뉴 흐름 연결 및 시스템 전체 시나리오 검증
- **범위**: 메인 메뉴에 전체 요약 정보(등록 시료 수, 총 재고, 전체 주문 수, 생산라인 대기 건수) 표시, 모든 하위 메뉴 연결
- **요약 정보 정의 (합리적 해석, `docs/design/phase8.md` 참고)**: "전체 주문 수"는 REJECTED를 포함한 전체 등록 주문 수(prd.md §4.1은 §4.5와 달리 REJECTED 제외를 명시하지 않음), "생산라인 대기 건수"는 `MonitoringService::orderCountSummary().producing`(대기+활성 생산 모두 포함) 재사용.
- **완료 기준**: "주문 접수 → 승인(재고부족) → 생산완료 → 출고"까지의 전체 흐름이 콘솔 상에서 End-to-End로 재현 가능 (Demonstrability 근거로 실행 로그 첨부), 동일 흐름을 자동화된 E2E 테스트로도 회귀 보호
- **구현 완료** (`log/phase8.md` 참고): `MonitoringService::mainMenuSummary()` 추가, `IView::showMainMenu` 추가하여 `main.cpp`의 `printMenu()` 자유 함수 제거, 메인 메뉴 루프마다 `productionQueue.advance()` 호출 추가(생산 라인 조회를 거치지 않아도 완료 자동 반영), `EndToEndTest` 신규로 전체 흐름 회귀 보호.

## Phase 9 — 코드 정리: `main.cpp` 중복 `advance()` 호출 제거

- **배경**: Phase 8 코드 리뷰에서 발견 — Phase 5 때 추가된 시작 시 1회 `productionQueue.advance()` 호출(`"재시작 시 밀린 생산완료를 즉시 반영"`)이, Phase 8에서 메인 루프 매 반복 시작에 `advance()`를 호출하도록 바뀌면서 완전히 중복되었다. 두 호출 사이에 시간이 흐르지 않으므로 동작상 문제는 없으나, 읽는 사람이 "왜 두 번 호출하는가"를 헷갈릴 수 있는 죽은 코드에 가깝다.
- **목표**: 새 기능 추가 없이 `main.cpp`의 중복 호출과 관련 주석을 제거해 가독성을 회복한다.
- **범위**: `main.cpp`에서 `ProductionQueue` 생성 직후의 단독 `advance()` 호출과 주석 삭제. 루프 내부의 `advance()` 호출(Phase 8에서 추가된 것)은 그대로 유지.
- **완료 기준**: 기존 70개 테스트 전체 회귀 없이 통과, 콘솔 실행 로그로 "생산 라인 조회를 거치지 않아도 재시작 시 생산완료가 자동 반영된다"는 Phase 8 시나리오가 여전히 동일하게 재현되는지 재확인.
- **구현 완료** (`log/phase9.md` 참고): `main.cpp`의 중복 `advance()` 호출과 주석 삭제. ctest 70/70 회귀 없음, 콘솔 실행으로 Phase 8과 동일 시나리오 재확인.

## Phase 10 — 콘솔 한글 출력 깨짐 수정

- **배경**: 실제 사용 환경(Windows 콘솔)에서 한글이 깨져 보이는 문제가 보고되었다. 원인은 UTF-8로 저장된 소스의 한글 문자열을 `printf`가 그대로 출력하지만, 콘솔의 활성 코드페이지가 UTF-8이 아니면(한국어 Windows 기본값은 보통 CP949) 잘못 해석되어 깨져 보인다. 지금까지 개발 중 확인에 쓴 Git Bash(mintty)는 항상 UTF-8이라 드러나지 않았던 환경 종속 버그다.
- **목표**: 새 기능 추가 없이 콘솔 코드페이지를 UTF-8로 고정해 한글이 항상 올바르게 표시되도록 한다.
- **범위**: `main.cpp` 진입점에서 `SetConsoleOutputCP`/`SetConsoleCP`(Windows API)로 코드페이지를 UTF-8(65001)로 고정. `#ifdef _WIN32`로 감싸 이식성 유지.
- **완료 기준**: 기존 70개 테스트 전체 회귀 없이 통과. `chcp 949`로 강제 변경 후 앱 실행 시 콘솔 코드페이지가 65001로 바뀌는지 확인(리다이렉트 캡처로는 mojibake 자체를 재현할 수 없으므로 간접 증거로 대체).
- **구현 완료** (`log/phase10.md` 참고): `main.cpp`에 `SetConsoleOutputCP`/`SetConsoleCP(CP_UTF8)` 추가. ctest 70/70 회귀 없음. `chcp 949` 강제 후 앱 실행 시 콘솔 코드페이지가 65001로 바뀜을 확인(간접 증거).

## Phase 11 — 테스트 커버리지 보강

- **배경**: `tc.md` 충분성 점검 중 발견된 3가지 공백.
  1. **`ConsoleView`(실제 `IView` 구현체)에 대한 자동화 테스트 전무**: 모든 Controller 테스트는 `StubView`로 위임만 확인하고, 실제 `printf` 포맷 출력은 각 Phase의 `log/phaseN.md` 수동 실행 로그로만 확인되어 왔다. `mvc` PoC에는 정확히 이 목적의 `ConsoleViewTest.cpp`(`testing::internal::CaptureStdout()`)가 이미 있으나 semi에는 이식되지 않았다.
  2. **`SampleRepository::search()`의 수치형 필드(생산시간/수율) 매칭 미검증**: Phase 2 설계에서 정한 "id/name/avgProductionTimeMinutes/yield 4개 필드 중 하나라도 일치" 규칙 중 수치형 2개 필드는 테스트가 없다.
  3. **`OrderController::approveOrder()`의 PRODUCING(재고 부족) 분기 메시지가 Controller 테스트에서 미검증**: `ApproveOrderShowsSuccessMessage`는 재고 충분(CONFIRMED) 케이스만 다룬다.
- **목표**: 새 기능 추가 없이 위 3가지 공백을 테스트로 채우고, `gcov`로 실제 라인 커버리지를 수치화한다.
- **범위**:
  - `tests/ConsoleViewTest.cpp` 신규 — `mvc` PoC의 stdout 캡처 패턴을 이식해 `ConsoleView`의 10개 `IView` 메서드 전부(빈 목록 케이스 포함) 검증
  - `tests/SampleRepositoryTest.cpp`에 `avgProductionTimeMinutes`/`yield` 값으로 검색하는 케이스 추가
  - `tests/OrderControllerTest.cpp`에 재고 부족(PRODUCING) 승인 시 "재고 부족, 생산 대기" 메시지가 전달되는 케이스 추가
  - **`gcov` 커버리지 계측 도입**: 이미 GCC(WinGet mingw64) 툴체인을 쓰고 있어 `gcov` 자체는 새 의존성 없이 바로 사용 가능. `gcovr`/`lcov` 같은 별도 리포트 도구는 도입하지 않는다(사람 확인 완료). `scripts/coverage.sh`(신규, `build.sh`와 별도 빌드 디렉토리 사용)로 `--coverage` 계측 빌드 → `ctest` → `gcov` 실행까지 자동화
- **완료 기준**: 신규 테스트 전부 통과, 기존 70개 테스트 전체 회귀 없음, `tc.md` 1부 인벤토리 갱신, `scripts/coverage.sh` 실행으로 `.gcov` 리포트가 생성되고 이번에 보강한 3개 영역(ConsoleView/검색/승인 분기)의 라인 커버리지가 실제로 오른 것을 확인
- **구현 완료** (`log/phase11.md` 참고): `ConsoleViewTest.cpp`(15개) 신규, `SampleRepositoryTest`/`OrderControllerTest`에 각 1개 추가(총 87개). `scripts/coverage.sh` 신규(gcov 계측). 보강 전/후 비교: `ConsoleView.cpp` 0%→100%, `OrderController.cpp` 94.59%→97.30%. `SampleRepository.cpp`는 95.83%로 **불변** — 신규 검색 케이스가 이미 실행되던 라인(OR 조건)을 다른 값으로 재실행한 것이라 라인 커버리지 도구로는 차이가 드러나지 않음(정직하게 기록, 브랜치 커버리지였다면 달랐을 것).

## Phase 12 — 분기/조건 커버리지 강화 (다양한 입력 기반 TC 확대)

- **배경**: Phase 11에서 `gcov` **라인** 커버리지만으로는 `SampleRepository::search()`의 4항 OR 조건처럼, "그 줄이 실행됐는가"와 "그 줄 안의 각 조건이 개별적으로 참/거짓을 만들어냈는가"가 다르다는 한계가 드러났다(`log/phase11.md` "발견한 사항"). 사용자가 다양한 입력으로 여러 논리 상황(분기/조건)에 대한 TC를 만들고 싶다고 요청했다.
- **목표**: `gcov`의 분기 계측(`-b`)을 켜서 실제로 무엇이 안 타는지 눈으로 확인하고, 소스 전체를 훑어 찾은 다중 조건(OR/AND) 분기들에 대해 "각 조건이 독립적으로 참/거짓의 결정 요인이 되는" 입력 조합을 표 기반(파라미터라이즈드) 테스트로 채운다.
- **범위**: `docs/design/phase12.md` 참고. 소스 전체의 `||`/`&&` 표현식을 전수 조사해 만든 분기 인벤토리, 우선순위별 보강 대상, `TEST_P` 활용 방침을 담는다.
- **완료 기준**: `scripts/coverage.sh -b`(분기 계측)로 대상 분기들의 양쪽 경로가 모두 최소 1회 실행됨을 확인, 신규 테스트 전부 통과, 기존 87개 테스트 회귀 없음, `tc.md`에 분기 커버리지 매트릭스(3부) 추가.

이 프로젝트의 마지막 Phase.

---

## PoC ↔ Phase 재사용 매핑

각 PoC 경로의 코드를 semi 프로젝트로 직접 이식(porting)하여 재사용한다. 아래는 어느 PoC의 코드를 어느 Phase에서 가져올지에 대한 매핑이다.

| PoC | 경로 | 재사용할 코드 | 관련 Phase |
|---|---|---|---|
| `mvc` | `C:\Users\User\mvc` | Model/View/Controller 골격, View 인터페이스(IView) 및 stub 처리 방식 | Phase 1, 2, 3 |
| `json_crud` | `C:\Users\User\json_crud` | JSON atomic write(`JsonDocument`), 경로 기반 CRUD, 변경 이력(History) 기록 코드 | Phase 1(영속성 계층), 2, 3 |
| `datamon` | `C:\Users\User\datamon` | 읽기 전용 조회 세션(모니터링) 구성 코드 | Phase 7(모니터링) |
| `DummyDataGenerator-djdj07` | `C:\Users\User\DummyDataGenerator-djdj07` | 스키마 기반 더미 데이터 생성기(valid/invalid, edge 케이스) 코드 → `tests/dummygen/`(`DummyGenerator`, `SchemaValidator`)으로 이식 완료 (Phase 2) | 각 Phase의 테스트 데이터 준비 시 |

> 각 PoC의 코드를 그대로 복사한 뒤 semi 프로젝트의 네임스페이스/도메인(Sample/Order)에 맞게 수정하는 순서로 진행한다. Phase 착수 시 설계 문서(`docs/design/phaseN.md`)에 "어느 파일을 어디서 가져와 무엇을 수정했는지"를 기록한다.

## 미결 사항 (착수 전 확인 필요)

- ~~데이터 저장 파일 형식 및 저장 위치~~ → 확정됨 (`docs/design/phase1.md` 참고: 단일 `data.json`, `semi/data/` 위치)
- ~~생산 완료의 시각 비교 판정 방식~~ → 확정됨 (`docs/design/phase5.md` 참고: 시작/완료 시각을 epoch seconds로 영속 저장, `ProductionQueue::advance()`가 호출 시점마다 비교·전이)
