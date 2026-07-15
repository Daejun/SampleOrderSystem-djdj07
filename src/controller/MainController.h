#pragma once

#include <string>

#include "MonitoringController.h"
#include "OrderController.h"
#include "ProductionController.h"
#include "SampleController.h"
#include "../view/IView.h"

class MainController {
public:
    MainController(IView& view, SampleController& sampleController, OrderController& orderController,
                    ProductionController& productionController, MonitoringController& monitoringController);

    void handleSelection(const std::string& input);
    bool isExitRequested() const { return exitRequested_; }

private:
    void runSampleMenu();
    void runOrderMenu();
    void runApprovalMenu();
    void runReleaseMenu();
    void runMonitoringMenu();

    IView& view_;
    SampleController& sampleController_;
    OrderController& orderController_;
    ProductionController& productionController_;
    MonitoringController& monitoringController_;
    bool exitRequested_ = false;
};
