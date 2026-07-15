#pragma once

#include "../model/MonitoringService.h"
#include "../view/IView.h"

class MonitoringController {
public:
    MonitoringController(MonitoringService& service, IView& view);

    void showOrderSummary();
    void showInventoryStatus();

private:
    MonitoringService& service_;
    IView& view_;
};
