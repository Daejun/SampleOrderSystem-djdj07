#include "MainController.h"

MainController::MainController(IView& view) : view_(view) {}

void MainController::handleSelection(const std::string& input) {
    if (input == "1") {
        view_.showMessage("[시료 관리] 미구현입니다.");
    } else if (input == "2") {
        view_.showMessage("[시료 주문] 미구현입니다.");
    } else if (input == "3") {
        view_.showMessage("[주문 승인/거절] 미구현입니다.");
    } else if (input == "4") {
        view_.showMessage("[모니터링] 미구현입니다.");
    } else if (input == "5") {
        view_.showMessage("[생산 라인 조회] 미구현입니다.");
    } else if (input == "6") {
        view_.showMessage("[출고 처리] 미구현입니다.");
    } else if (input == "0") {
        exitRequested_ = true;
    } else {
        view_.showError("알 수 없는 선택입니다: " + input);
    }
}
