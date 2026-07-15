#include "MainController.h"

#include <cstdio>
#include <iostream>
#include <stdexcept>

namespace {

void printSampleMenu() {
    std::printf("\n--- 시료 관리 ---\n");
    std::printf("[1] 시료 등록\n");
    std::printf("[2] 시료 목록\n");
    std::printf("[3] 시료 검색\n");
    std::printf("[0] 뒤로가기\n");
    std::printf("선택 > ");
}

}  // namespace

MainController::MainController(IView& view, SampleController& sampleController)
    : view_(view), sampleController_(sampleController) {}

void MainController::handleSelection(const std::string& input) {
    if (input == "1") {
        runSampleMenu();
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

void MainController::runSampleMenu() {
    std::string line;
    while (true) {
        printSampleMenu();
        if (!std::getline(std::cin, line)) {
            return;
        }

        if (line == "1") {
            std::string id;
            std::string name;
            std::string avgStr;
            std::string yieldStr;

            std::printf("시료 ID > ");
            std::getline(std::cin, id);
            std::printf("이름 > ");
            std::getline(std::cin, name);
            std::printf("평균 생산시간(min/ea) > ");
            std::getline(std::cin, avgStr);
            std::printf("수율(0~1) > ");
            std::getline(std::cin, yieldStr);

            try {
                const double avg = std::stod(avgStr);
                const double yield = std::stod(yieldStr);
                sampleController_.registerSample(id, name, avg, yield);
            } catch (const std::exception&) {
                view_.showError("생산시간/수율은 숫자로 입력해야 합니다.");
            }
        } else if (line == "2") {
            sampleController_.listSamples();
        } else if (line == "3") {
            std::string keyword;
            std::printf("검색어 > ");
            std::getline(std::cin, keyword);
            sampleController_.searchSamples(keyword);
        } else if (line == "0") {
            return;
        } else {
            view_.showError("알 수 없는 선택입니다: " + line);
        }
    }
}
