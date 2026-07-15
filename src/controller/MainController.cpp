#include "MainController.h"

#include "OrderInput.h"

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

void printOrderMenu() {
    std::printf("\n--- 시료 주문 ---\n");
    std::printf("[1] 주문 접수\n");
    std::printf("[2] 주문 목록\n");
    std::printf("[0] 뒤로가기\n");
    std::printf("선택 > ");
}

void printApprovalMenu() {
    std::printf("\n--- 주문 승인/거절 ---\n");
    std::printf("[1] 접수된 주문 목록 (RESERVED)\n");
    std::printf("[2] 주문 승인\n");
    std::printf("[3] 주문 거절\n");
    std::printf("[0] 뒤로가기\n");
    std::printf("선택 > ");
}

}  // namespace

MainController::MainController(IView& view, SampleController& sampleController, OrderController& orderController)
    : view_(view), sampleController_(sampleController), orderController_(orderController) {}

void MainController::handleSelection(const std::string& input) {
    if (input == "1") {
        runSampleMenu();
    } else if (input == "2") {
        runOrderMenu();
    } else if (input == "3") {
        runApprovalMenu();
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

void MainController::runOrderMenu() {
    std::string line;
    while (true) {
        printOrderMenu();
        if (!std::getline(std::cin, line)) {
            return;
        }

        if (line == "1") {
            std::string sampleId;
            std::string customerName;
            std::string quantityStr;

            std::printf("시료 ID > ");
            std::getline(std::cin, sampleId);
            std::printf("고객명 > ");
            std::getline(std::cin, customerName);
            std::printf("주문 수량 > ");
            std::getline(std::cin, quantityStr);

            int quantity = 0;
            if (!orderinput::parsePositiveQuantity(quantityStr, quantity)) {
                view_.showError("주문 수량은 0보다 큰 정수로 입력해야 합니다.");
                continue;
            }
            orderController_.reserveOrder(sampleId, customerName, quantity);
        } else if (line == "2") {
            orderController_.listOrders();
        } else if (line == "0") {
            return;
        } else {
            view_.showError("알 수 없는 선택입니다: " + line);
        }
    }
}

void MainController::runApprovalMenu() {
    std::string line;
    while (true) {
        printApprovalMenu();
        if (!std::getline(std::cin, line)) {
            return;
        }

        if (line == "1") {
            orderController_.listReservedOrders();
        } else if (line == "2") {
            std::string orderNumber;
            std::printf("주문번호 > ");
            std::getline(std::cin, orderNumber);
            orderController_.approveOrder(orderNumber);
        } else if (line == "3") {
            std::string orderNumber;
            std::printf("주문번호 > ");
            std::getline(std::cin, orderNumber);
            orderController_.rejectOrder(orderNumber);
        } else if (line == "0") {
            return;
        } else {
            view_.showError("알 수 없는 선택입니다: " + line);
        }
    }
}
