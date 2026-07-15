# Phase 1 설계: 프로젝트 스캐폴딩

`PLAN.md`의 Phase 1 목표(MVC 구조 + CMake 빌드 체계를 갖춘, 빌드/실행만 되는 빈 콘솔 앱)에 대한 상세 설계.

## 확정된 결정 사항 (사람 확인 완료)

- **저장 파일 구성**: 단일 파일 `data.json` 안에 `samples`, `orders` 두 키로 관리한다 (파일 분리 안 함).
- **생산 진행 상태 영속화**: 별도 큐 파일 없이, `orders` 배열의 각 주문 항목에 `productionStartedAt`/`expectedCompletionAt` 필드로 포함한다. FIFO 순서는 배열 내 등장 순서로 판단한다 (Phase 5에서 상세화).
- **저장 위치**: `semi/data/data.json` (런타임 생성 파일, `.gitignore` 대상. 샘플/초기 데이터는 커밋하지 않음).

## 디렉토리 구조

```
semi/
  CMakeLists.txt
  src/
    model/
      Sample.h / Sample.cpp            (Phase 2에서 구현)
      Order.h / Order.cpp              (Phase 3에서 구현)
    view/
      IView.h                          (mvc PoC의 IView.h 이식 후 Sample/Order 대상으로 확장)
      ConsoleView.h / ConsoleView.cpp
    controller/
      MainController.h / MainController.cpp   (메뉴 스텁만 Phase 1에서 구현)
    persistence/
      JsonDocument.h / JsonDocument.cpp        (json_crud PoC에서 이식, atomic write)
    main.cpp
  tests/
    CMakeLists.txt
    PlaceholderTest.cpp
  data/                                (런타임 생성, .gitignore)
  docs/design/phase1.md                (본 문서)
```

## CMake 구성

- 최상위 `CMakeLists.txt`: C++17, `FetchContent`로 `nlohmann_json`(v3.11.3, `json_crud` PoC와 동일 버전) 선언 후 `add_subdirectory(src)` / `add_subdirectory(tests)`.
- `src/`: `persistence` 정적 라이브러리(`sample_order_persistence`, nlohmann_json 링크) + `sample_order_app` 실행 파일 (`json_crud`의 `json_crud_core` / `mvc`의 `mvc_app` 구성 방식 혼합).
- `tests/CMakeLists.txt`: `FetchContent`로 `googletest`(v1.15.2, 두 PoC와 동일 버전) 선언, `gtest_discover_tests` 사용 (mvc/json_crud 패턴과 동일).

## 이식 대상 파일 (PoC 코드 직접 복사 후 수정)

| 원본 | 대상 | 수정 포인트 |
|---|---|---|
| `mvc/src/view/IView.h` | `src/view/IView.h` | `Item` 참조 제거, Phase 2/3에서 Sample/Order 대상 메서드로 확장 |
| `mvc/tests/CMakeLists.txt` | `tests/CMakeLists.txt` | 파일 목록을 semi 테스트 파일로 교체, googletest FetchContent 블록은 그대로 유지 |
| `json_crud/src/JsonDocument.h`, `JsonDocument.cpp` | `src/persistence/JsonDocument.h/.cpp` | `namespace jsoncrud` → 프로젝트에 맞는 네임스페이스로 변경 (예: 유지 또는 `sampleorder`), 그 외 atomic write 로직은 그대로 사용 |
| `json_crud/CMakeLists.txt`의 FetchContent 블록 | 최상위 `CMakeLists.txt` | nlohmann_json 선언부만 그대로 가져옴 |

## Phase 1 범위에서 실제로 만드는 것

1. 위 CMake 빌드 스켈레톤 (`cmake -S . -B build` / `cmake --build build` 성공)
2. `JsonDocument`만 이식 — 아직 Sample/Order 도메인과 연결하지 않음 (Phase 2의 `SampleRepository`가 최초로 사용)
3. 메인 메뉴 스텁: 메뉴 항목(시료 관리 / 주문 접수 / 승인·거절 / 모니터링 / 출고 처리 / 생산 라인 조회)만 출력하고, 각 선택 시 "미구현" 메시지만 표시
4. `PlaceholderTest` 1개로 `ctest` 통과 확인 (mvc PoC의 TDD 1단계와 동일)

## Phase 1에서 하지 않는 것 (다음 Phase로 이연)

- `Sample`/`Order` 엔티티, `SampleRepository`/`OrderRepository` 구현 (Phase 2, 3)
- `data.json`의 실제 필드 스키마 확정 (예: Sample: id/name/avgProductionTime/yield/stock, Order: orderId/sampleId/customer/qty/status/productionStartedAt/expectedCompletionAt) — 각 Phase 설계 시 확정
- `ConsoleView`/`Controller`의 실제 기능 로직 (Phase 2 이후 mvc 패턴을 참고해 도메인에 맞게 재작성)

## 완료 기준 (PLAN.md와 동일)

- `cmake --build` 성공
- `ctest` 통과 (placeholder 테스트 포함)
- 메뉴 스텁 실행 확인 (콘솔 실행 로그로 Demonstrability 제공)
