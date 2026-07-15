#!/usr/bin/env bash
# --coverage 계측 빌드로 gtest를 실행한 뒤 gcov 리포트를 생성한다.
# scripts/build.sh와 별도 빌드 디렉토리(build-coverage/)를 사용해 평소 빌드 산출물과 섞이지 않게 한다.
# (Git Bash mingw64 PATH 충돌 이슈는 scripts/build.sh와 동일 — 상세는 log/phase2.md 참고)

set -euo pipefail

MINGW_BIN="/c/Users/User/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin"
export PATH="$MINGW_BIN:$PATH"

cd "$(dirname "$0")/.."

cmake -S . -B build-coverage -G "MinGW Makefiles" -DENABLE_COVERAGE=ON
cmake --build build-coverage
ctest --test-dir build-coverage --output-on-failure

cd build-coverage
# .gcda가 생성된 각 디렉토리를 찾아 -o로 지정, 소스 상대경로 유지(-p)로 gcov 실행
find . -name '*.gcda' -exec dirname {} \; | sort -u | while read -r dir; do
    gcov -b -p -o "$dir" "$dir"/*.gcda >/dev/null
done
echo "커버리지 리포트(.gcov)가 build-coverage/ 에 생성되었습니다."
