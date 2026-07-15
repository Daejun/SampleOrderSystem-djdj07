"""PostToolUse(Write|Edit) 훅: CLAUDE.md의 문서/테스트 동기화 규칙을 mtime 비교로 상기시킨다.

- tests/*.cpp가 수정되면 tc.md가 그보다 최신인지 확인
- src/model/*.cpp(핵심 로직)가 수정되면 대응 테스트가 그보다 최신인지 확인
- src/**/*.cpp|h가 수정되면 log/phase*.md가 그보다 최신인지 확인

세 규칙 모두 "경고만" 한다 (빌드/명령을 막지 않음). mtime 비교는 근사치이므로 실제로 문서를
갱신했는지는 사람/모델이 최종 판단한다.
"""

import json
import os
import re
import sys
import glob


def newest_mtime(patterns):
    latest = 0
    for pattern in patterns:
        for path in glob.glob(pattern):
            try:
                mtime = os.path.getmtime(path)
                if mtime > latest:
                    latest = mtime
            except OSError:
                pass
    return latest


def main():
    try:
        data = json.load(sys.stdin)
    except Exception:
        return

    tool_input = data.get("tool_input") or {}
    tool_response = data.get("tool_response") or {}
    path = tool_input.get("file_path") or tool_response.get("filePath") or ""
    if not path:
        return

    norm = path.replace("\\", "/")
    try:
        edited_mtime = os.path.getmtime(path)
    except OSError:
        return

    messages = []

    if re.search(r"(^|/)tests/.*\.cpp$", norm):
        try:
            tc_mtime = os.path.getmtime("tc.md")
        except OSError:
            tc_mtime = 0
        if edited_mtime > tc_mtime:
            messages.append(
                f"'{norm}'이(가) 수정되었지만 tc.md가 그보다 최신이 아닙니다. "
                "테스트 케이스를 추가/변경했다면 tc.md도 함께 갱신하세요."
            )

    if re.search(r"(^|/)src/model/.*\.cpp$", norm):
        test_mtime = newest_mtime(["tests/*.cpp"])
        if edited_mtime > test_mtime:
            messages.append(
                f"핵심 로직 파일 '{norm}'이(가) 수정되었지만, 이보다 최신인 테스트 파일이 없습니다. "
                "상태 전이/재고 계산/생산 큐 로직 변경 시 반드시 대응 테스트를 작성·갱신하세요."
            )

    if re.search(r"(^|/)src/.*\.(cpp|h)$", norm):
        log_mtime = newest_mtime(["log/phase*.md"])
        if edited_mtime > log_mtime:
            messages.append(
                f"'{norm}'이(가) 수정되었지만 log/ 아래 Phase 로그가 그보다 최신이 아닙니다. "
                "Phase 구현이 끝났다면 log/phaseN.md에 4대 Verify 결과를 기록하세요."
            )

    if messages:
        combined = "\n".join(messages)
        print(json.dumps({
            "systemMessage": combined,
            "hookSpecificOutput": {
                "hookEventName": "PostToolUse",
                "additionalContext": combined,
            },
        }))


if __name__ == "__main__":
    main()
