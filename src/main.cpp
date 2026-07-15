#include <cstdio>
#include <iostream>
#include <string>

#include "controller/MainController.h"
#include "controller/OrderController.h"
#include "controller/SampleController.h"
#include "model/OrderRepository.h"
#include "model/SampleRepository.h"
#include "persistence/JsonStore.h"
#include "view/ConsoleView.h"

namespace {

void printMenu() {
    std::printf("\n=== 반도체 시료 생산주문관리 시스템 ===\n");
    std::printf("[1] 시료 관리\n");
    std::printf("[2] 시료 주문\n");
    std::printf("[3] 주문 승인/거절\n");
    std::printf("[4] 모니터링\n");
    std::printf("[5] 생산 라인 조회\n");
    std::printf("[6] 출고 처리\n");
    std::printf("[0] 종료\n");
    std::printf("선택 > ");
}

}  // namespace

int main() {
    sampleorder::JsonStore store("data/data.json");
    store.ensureLoaded();

    SampleRepository sampleRepository(store);
    OrderRepository orderRepository(store, sampleRepository);
    ConsoleView view;
    SampleController sampleController(sampleRepository, view);
    OrderController orderController(orderRepository, view);
    MainController controller(view, sampleController, orderController);

    std::string line;
    while (!controller.isExitRequested()) {
        printMenu();
        if (!std::getline(std::cin, line)) {
            break;
        }
        controller.handleSelection(line);
    }

    return 0;
}
