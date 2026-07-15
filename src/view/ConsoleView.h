#pragma once

#include "IView.h"

class ConsoleView : public IView {
public:
    void showMessage(const std::string& message) override;
    void showError(const std::string& message) override;
    void showSamples(const std::vector<Sample>& samples) override;
    void showSample(const Sample& sample) override;
    void showOrders(const std::vector<Order>& orders) override;
    void showOrder(const Order& order) override;
    void showProductionStatus(const std::optional<Order>& active, const std::vector<Order>& waiting) override;
    void showOrderCountSummary(const OrderCountSummary& summary) override;
    void showInventoryStatus(const std::vector<SampleInventoryStatus>& statuses) override;
    void showMainMenu(const MainMenuSummary& summary) override;
};
