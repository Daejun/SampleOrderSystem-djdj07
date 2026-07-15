#include "MonitoringController.h"

MonitoringController::MonitoringController(MonitoringService& service, IView& view)
    : service_(service), view_(view) {}

void MonitoringController::showOrderSummary() {
    view_.showOrderCountSummary(service_.orderCountSummary());
}

void MonitoringController::showInventoryStatus() {
    view_.showInventoryStatus(service_.inventoryStatus());
}
