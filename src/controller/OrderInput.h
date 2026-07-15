#pragma once

#include <string>

namespace orderinput {

// 성공 시 true를 반환하고 quantity에 파싱된 값(0보다 큼)을 저장한다.
// 실패(숫자가 아님, 0 이하, 뒤에 불필요한 문자가 붙음) 시 false를 반환한다.
bool parsePositiveQuantity(const std::string& raw, int& quantity);

} // namespace orderinput
