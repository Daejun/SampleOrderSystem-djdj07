"""PreToolUse(Bash) 훅: cmake/ctest를 scripts/build.sh 없이 직접 실행하려 할 때 권고 메시지를 띄운다.

Git Bash 내장 mingw64와 WinGet mingw64의 PATH 충돌(log/phase2.md 참고)로 직접 실행 시
STATUS_ENTRYPOINT_NOT_FOUND(0xc0000139)가 날 수 있다. 차단하지 않고 권고만 한다
(permissionDecision: allow).
"""

import json
import re
import sys


def main():
    try:
        data = json.load(sys.stdin)
    except Exception:
        return

    command = (data.get("tool_input") or {}).get("command") or ""
    if "build.sh" in command:
        return

    looks_like_direct_build = bool(
        re.search(r"\bctest\b", command)
        or re.search(r"\bcmake\b[^|;&]*--build", command)
        or re.search(r"\bcmake\b[^|;&]*-S\b", command)
    )
    if not looks_like_direct_build:
        return

    msg = (
        "cmake/ctest를 scripts/build.sh 없이 직접 실행하려 합니다. 이 프로젝트는 Git Bash 내장 "
        "mingw64와 WinGet mingw64의 PATH 충돌로 직접 실행 시 0xc0000139로 실패할 수 있습니다 "
        "(log/phase2.md 참고). 가능하면 `bash scripts/build.sh`를 사용하세요."
    )
    print(json.dumps({
        "systemMessage": msg,
        "hookSpecificOutput": {
            "hookEventName": "PreToolUse",
            "permissionDecision": "allow",
            "permissionDecisionReason": msg,
            "additionalContext": msg,
        },
    }))


if __name__ == "__main__":
    main()
