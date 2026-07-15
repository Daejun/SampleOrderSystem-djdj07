#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../model/MonitoringService.h"
#include "../model/Order.h"
#include "../model/Sample.h"

class IView {
public:
    virtual ~IView() = default;
    virtual void showMessage(const std::string& message) = 0;
    virtual void showError(const std::string& message) = 0;
    virtual void showSamples(const std::vector<Sample>& samples) = 0;
    virtual void showSample(const Sample& sample) = 0;
    virtual void showOrders(const std::vector<Order>& orders) = 0;
    virtual void showOrder(const Order& order) = 0;
    virtual void showProductionStatus(const std::optional<Order>& active, const std::vector<Order>& waiting) = 0;
    virtual void showOrderCountSummary(const OrderCountSummary& summary) = 0;
    virtual void showInventoryStatus(const std::vector<SampleInventoryStatus>& statuses) = 0;
    virtual void showMainMenu(const MainMenuSummary& summary) = 0;
};
