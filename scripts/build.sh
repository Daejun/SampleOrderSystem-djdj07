#!/usr/bin/env bash
# Git Bash(MSYS)에는 Git 번들 mingw64(/mingw64/bin)가 WinGet으로 설치한
# mingw64보다 PATH 앞쪽에 잡혀 있다. 두 mingw64의 libstdc++-6.dll/libgcc_s_seh-1.dll이
# ABI가 달라, 실제 컴파일에 쓰인 WinGet 툴체인으로 링크한 실행 파일을 Git 번들
# libstdc++로 로드하면 STATUS_ENTRYPOINT_NOT_FOUND(0xc0000139)로 즉시 죽는다.
# (원인 분석: log/phase2.md 참고)
#
# 이 스크립트는 WinGet mingw64/bin을 PATH 맨 앞에 둔 채로 CMake configure/build/ctest를
# 실행해 위 문제를 피한다. 로컬에서 직접 cmake/ctest를 칠 때도 반드시 이 PATH 순서를
# 지켜야 한다.

set -euo pipefail

MINGW_BIN="/c/Users/User/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin"
export PATH="$MINGW_BIN:$PATH"

cd "$(dirname "$0")/.."

cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
