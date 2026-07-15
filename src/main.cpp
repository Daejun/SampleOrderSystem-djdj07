#include <iostream>
#include <string>

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
