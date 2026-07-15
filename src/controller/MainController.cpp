#include "MainController.h"

#include "OrderInput.h"
#include "SubMenu.h"

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

void printReleaseMenu() {
    std::printf("\n--- 출고 처리 ---\n");
    std::printf("[1] 출고 대상 주문 목록 (CONFIRMED)\n");
    std::printf("[2] 출고 처리\n");
    std::printf("[0] 뒤로가기\n");
    std::printf("선택 > ");
}

void printMonitoringMenu() {
    std::printf("\n--- 모니터링 ---\n");
    std::printf("[1] 주문량 확인\n");
    std::printf("[2] 재고량 확인\n");
    std::printf("[0] 뒤로가기\n");
    std::printf("선택 > ");
}

}  // namespace

MainController::MainController(IView& view, SampleController& sampleController, OrderController& orderController,
                                ProductionController& productionController,
                                MonitoringController& monitoringController)
    : view_(view),
      sampleController_(sampleController),
      orderController_(orderController),
      productionController_(productionController),
      monitoringController_(monitoringController) {}

void MainController::handleSelection(const std::string& input) {
    if (input == "1") {
        runSampleMenu();
    } else if (input == "2") {
        runOrderMenu();
    } else if (input == "3") {
        runApprovalMenu();
    } else if (input == "4") {
        runMonitoringMenu();
    } else if (input == "5") {
        productionController_.showStatus();
    } else if (input == "6") {
        runReleaseMenu();
    } else if (input == "0") {
        exitRequested_ = true;
    } else {
        view_.showError("알 수 없는 선택입니다: " + input);
    }
}

void MainController::runSampleMenu() {
    submenu::run(printSampleMenu, view_, {
        {"1", [this]() {
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
        }},
        {"2", [this]() { sampleController_.listSamples(); }},
        {"3", [this]() {
            std::string keyword;
            std::printf("검색어 > ");
            std::getline(std::cin, keyword);
            sampleController_.searchSamples(keyword);
        }},
    });
}

void MainController::runOrderMenu() {
    submenu::run(printOrderMenu, view_, {
        {"1", [this]() {
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
                return;
            }
            orderController_.reserveOrder(sampleId, customerName, quantity);
        }},
        {"2", [this]() { orderController_.listOrders(); }},
    });
}

void MainController::runApprovalMenu() {
    submenu::run(printApprovalMenu, view_, {
        {"1", [this]() { orderController_.listReservedOrders(); }},
        {"2", [this]() {
            std::string orderNumber;
            std::printf("주문번호 > ");
            std::getline(std::cin, orderNumber);
            orderController_.approveOrder(orderNumber);
        }},
        {"3", [this]() {
            std::string orderNumber;
            std::printf("주문번호 > ");
            std::getline(std::cin, orderNumber);
            orderController_.rejectOrder(orderNumber);
        }},
    });
}

void MainController::runReleaseMenu() {
    submenu::run(printReleaseMenu, view_, {
        {"1", [this]() { orderController_.listConfirmedOrders(); }},
        {"2", [this]() {
            std::string orderNumber;
            std::printf("주문번호 > ");
            std::getline(std::cin, orderNumber);
            orderController_.releaseOrder(orderNumber);
        }},
    });
}

void MainController::runMonitoringMenu() {
    submenu::run(printMonitoringMenu, view_, {
        {"1", [this]() { monitoringController_.showOrderSummary(); }},
        {"2", [this]() { monitoringController_.showInventoryStatus(); }},
    });
}
