#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "controller/MainController.h"
#include "controller/MonitoringController.h"
#include "controller/OrderController.h"
#include "controller/ProductionController.h"
#include "controller/SampleController.h"
#include "model/MonitoringService.h"
#include "model/OrderRepository.h"
#include "model/ProductionQueue.h"
#include "model/SampleRepository.h"
#include "persistence/JsonStore.h"
#include "view/ConsoleView.h"

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // 출력: printf로 내보내는 UTF-8 바이트를 올바르게 렌더링
    SetConsoleCP(CP_UTF8);        // 입력: 고객명/검색어 등 한글 입력도 올바르게 해석
#endif

    sampleorder::JsonStore store("data/data.json");
    store.ensureLoaded();

    SampleRepository sampleRepository(store);
    OrderRepository orderRepository(store, sampleRepository);
    ProductionQueue productionQueue(store, sampleRepository, orderRepository);

    MonitoringService monitoringService(sampleRepository, orderRepository);

    ConsoleView view;
    SampleController sampleController(sampleRepository, view);
    OrderController orderController(orderRepository, view);
    ProductionController productionController(productionQueue, view);
    MonitoringController monitoringController(monitoringService, view);
    MainController controller(view, sampleController, orderController, productionController, monitoringController);

    std::string line;
    while (!controller.isExitRequested()) {
        productionQueue.advance();  // 생산 라인 조회를 거치지 않아도 완료가 지연 없이 반영되도록 매번 확인
        view.showMainMenu(monitoringService.mainMenuSummary());
        if (!std::getline(std::cin, line)) {
            break;
        }
        controller.handleSelection(line);
    }

    return 0;
}
