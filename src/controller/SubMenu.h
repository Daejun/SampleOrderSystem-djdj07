#pragma once

#include <functional>
#include <map>
#include <string>

#include "../view/IView.h"

// MainController의 서브메뉴 루프("메뉴 출력 -> getline -> 문자열 분기 -> "0"이면 종료 -> 그 외 오류")
// 중복(Rule of Three)을 제거하기 위한 도메인 비종속 공통 헬퍼.
namespace submenu {

using Handler = std::function<void()>;

// printMenu 출력 후 한 줄 입력을 받아 handlers에서 찾아 실행하기를 반복한다.
// "0"은 handlers에 등록하지 않아도 항상 종료로 처리한다. handlers에 없는 입력이면 view.showError.
// std::cin이 EOF(예: 파이프 입력 종료)면 즉시 종료한다.
void run(const std::function<void()>& printMenu, IView& view, const std::map<std::string, Handler>& handlers);

} // namespace submenu
