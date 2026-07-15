# CLAUDE.md

이 파일은 이 저장소에서 작업하는 Claude Code에게 제공되는 가이드입니다. 상세 기능 명세는 `prd.md`를 참조하세요.

## 역할 분리 원칙

| 주체 | 역할 |
|---|---|
| **AI (Claude Code)** | 설계 도움, 초안 작성, 코드 구현, 테스트 작성·실행, 작업 보고서 작성 |
| **사람** | 설계·기획 결정, 요구사항 확정, 최종 검토·승인, 책임 |

- AI는 스스로 판단해서 요구사항 범위를 확장·축소하지 않는다. 설계 방향에 영향을 주는 모호함은 반드시 먼저 질문한다.
- 지시받은 Phase/작업 범위를 벗어난 개발(Scope Creep)을 하지 않는다.
- 작업 완료를 "다 됐다"고 낙관적으로 보고하지 않는다. 실제로 검증한 내용과 검증하지 못한 내용을 구분해서 보고한다.

## 프로젝트 개요

"반도체 시료 생산주문관리 시스템" (SampleOrderSystem) — 콘솔(CLI) 기반 애플리케이션.
가상의 반도체 회사 S-Semi의 시료 주문 접수 → 승인/거절 → 생산 → 출고 흐름을 관리한다.

원본 과제 자료는 `res/[CRA_AI] Day3_개인과제_반도체시료관리.pdf`에 있다.

## 기술 스택

- 언어/런타임: C++
- 빌드 도구: CMake
- 테스트 프레임워크: gtest (Google Test)
- 주요 라이브러리: (필요 시 별도 제안·승인 후 추가)

새로운 라이브러리 도입이 필요하면 먼저 사람에게 제안하고 승인받은 후 진행한다.

### 빌드 환경 주의사항 (Git Bash PATH 충돌)

Git Bash(MSYS)에는 Git 번들 mingw64(`/mingw64/bin`)가 WinGet으로 설치한 mingw64보다 PATH 앞쪽에
잡혀 있다. 두 mingw64의 `libstdc++-6.dll`/`libgcc_s_seh-1.dll`은 ABI가 달라, WinGet 툴체인으로
컴파일·링크한 실행 파일을 Git 번들 `libstdc++`로 로드하면 `STATUS_ENTRYPOINT_NOT_FOUND`(종료 코드
`0xc0000139`)로 즉시 죽는다. `cmake --build`/`ctest`가 이 문제로 실패한 근본 원인 분석은
`log/phase2.md`를 참고한다.

**반드시 `scripts/build.sh`로 빌드/테스트를 실행한다.** 이 스크립트가 WinGet mingw64/bin을 PATH
맨 앞에 두고 configure → build → ctest를 수행하므로 PATH 충돌을 피할 수 있다. 직접 `cmake`/`ctest`를
칠 경우에도 동일하게 PATH 순서를 맞춰야 한다.

## 프로젝트 문서 구조

작업 시 아래 문서를 우선순위대로 참조한다. 문서 간 내용이 충돌하거나 모호하면 임의로 판단하지 말고 먼저 사람에게 확인한다.

```
CLAUDE.md          # 이 파일 - Agent 행동 규칙
prd.md             # 전체 기능 명세 및 도메인 규칙
PLAN.md            # Phase별 개발 계획 (아직 미작성 — 최초 Phase 분해 시 작성)
docs/design/phaseN.md  # Phase별 상세 설계 문서 (아직 미작성 — 각 Phase 설계 시 작성)
res/               # 원본 과제 PDF
```

> `PLAN.md`와 `docs/design/`은 아직 존재하지 않는다. 최초 Phase 분해 작업 시 사람과 협의하여 작성한다.

## 아키텍처 원칙

- **MVC 구조**를 따른다: Model / Controller / View 패키지로 역할을 명확히 분리한다.
  - Model: Sample, Order 등 도메인 엔티티 및 상태 전이 로직, 생산 큐
  - Controller: 메뉴 입력 처리, 유스케이스 흐름 제어
  - View: 콘솔 출력 포맷팅 (메뉴, 목록, 상태 표시)
- 데이터 영속성 계층은 별도 모듈로 분리하여 저장 방식(파일/JSON/DB 등) 교체가 쉬워야 한다.
- 신규 기능 추가 전 `prd.md`의 상태 전이 규칙(RESERVED/REJECTED/PRODUCING/CONFIRMED/RELEASE)을 우선 확인한다.

## 핵심 도메인 규칙 (prd.md 요약)

- 시료(Sample): 시료 ID, 이름, 평균 생산시간, 수율, 현재 재고를 가진다.
- 주문(Order) 상태 전이:
  - RESERVED --승인(재고 충분)--> CONFIRMED --출고--> RELEASE
  - RESERVED --승인(재고 부족)--> PRODUCING --생산완료--> CONFIRMED --출고--> RELEASE
  - RESERVED --거절--> REJECTED (모니터링 제외)
- 생산 라인은 단일 라인, FIFO 큐로 동작한다.
  - 실 생산량 = `ceil(부족분 / 수율)`
  - 총 생산 시간 = `평균 생산시간 * 실 생산량`
- 모니터링 시 REJECTED는 무효 주문으로 집계에서 제외한다.
- 재고 상태 판정: 여유 / 부족 / 고갈(0) 세 구간으로 표기한다.

## Verify 체계 (작업 완료 전 필수)

Phase/기능 구현이 끝나면 사람에게 보고하기 전에 아래를 스스로 점검한다. 네 가지를 통과하지 못한 상태에서 "완료되었습니다"라고 보고하지 않는다.

1. **Test Verify**: gtest로 작성한 관련 단위 테스트를 직접 빌드·실행하여 통과를 확인한다. 테스트를 임의로 완화하거나 우회해서 통과시키지 않는다.
   - 특히 상태 전이(RESERVED/REJECTED/PRODUCING/CONFIRMED/RELEASE), 재고 계산, 생산 큐(FIFO) 로직은 반드시 테스트로 검증한다.
2. **Compliance Verify**: 구현 결과가 `prd.md`의 요구사항(§4 기능 명세, §8 TODO)을 모두 충족하는지 체크리스트로 확인한다.
3. **Document Consistency Verify**: 구현 시작 전, `CLAUDE.md`/`prd.md`/`PLAN.md` 간 충돌·모호함이 없는지 확인한다. 있다면 임의로 해석하지 말고 먼저 질문한다.
4. **Demonstrability**: 콘솔 실행 로그, 재현 커맨드(빌드·실행·테스트 명령) 등을 남겨 사람이 직접 재현·확인할 수 있게 한다. "테스트를 통과했다"는 말로 대체하지 않는다.

각 Phase가 완료되면 위 1~4 항목을 정리한 결과를 `log/phaseN.md`로 기록·저장한다 (예: `log/phase1.md`). 최소한 아래 내용을 포함한다.

- 구현 내용 요약 (파일별 변경 사항)
- Test Verify 결과 (빌드·테스트 실행 로그 포함)
- Compliance Verify 결과 (설계 문서/PLAN.md 요구사항 대조표)
- Document Consistency Verify 결과 (충돌 발견 여부)
- Demonstrability (콘솔 실행 로그, 재현 커맨드)
- 검증하지 못한 내용 (있다면 명시)

사람 리뷰 전에 로그부터 작성하고, 리뷰 후 커밋 시 해당 Phase 구현 코드와 `log/phaseN.md`를 함께 커밋한다.

## PoC 선행 개발 (prd.md §7 기반)

본 구현 전에 아래 항목은 별도 PoC로 먼저 검증한다 (별도 Repository/디렉토리). PoC 완료 후 원리를 완전히 이해한 뒤에만 본 프로젝트 코드에 반영한다. PoC 단계에서는 Phase 분해·Verify 등 본 프로젝트의 엄격한 규칙을 적용하지 않아도 된다.

| PoC 항목 | 설명 |
|---|---|
| MVC 스켈레톤 코드 | Model/Controller/View 디렉토리 구조 및 역할 분리 (C++ 기준 헤더/소스 분리 포함) |
| 데이터 영속성 처리 | 파일/JSON/DB 등 방식으로 데이터 저장·조회 구조 구현 (CRUD 포함) |
| 데이터 모니터링 Tool | 저장된 데이터 상태를 콘솔에서 실시간 조회하는 관리자 도구 |
| Dummy 데이터 생성 Tool | 테스트용 Dummy Data 생성 및 저장소 반영 도구 |

- 익숙하지 않은 라이브러리(예: JSON 파싱 라이브러리)나 CMake 설정 방식은 PoC로 먼저 검증할 것을 제안한다.

위의 개발항목은 기 개발되어 있음,
C:\Users\User\json_crud
C:\Users\User\mvc
C:\Users\User\DummyDataGenerator-djdj07
C:\Users\User\datamon

아래 작업 목록은 위의 개발된 poc를 이용하여 작업한다.

## 개발 및 커밋 규칙

- 기능 단위로 의미 있는 커밋을 남긴다 (한 커밋에 여러 기능을 섞지 않는다).
- 핵심 로직(상태 전이, 재고 계산, 생산 큐)은 반드시 테스트를 작성한다.
- 불필요한 추상화나 미사용 코드를 추가하지 않는다.
- 화면 UI는 `prd.md`의 예시를 참고하되 자유롭게 구성 가능하다 (고정 스펙 아님).

## 참고 문서

- `prd.md` — 기능 명세 및 작업 목록(TODO)
- `PLAN.md` — Phase별 개발 계획
- `docs/design/` — Phase별 상세 설계 문서 (각 Phase 착수 시 작성)
- `log/` — Phase별 구현 완료 및 Verify 결과 기록 (각 Phase 완료 시 작성)
- `res/` — 원본 과제 PDF
