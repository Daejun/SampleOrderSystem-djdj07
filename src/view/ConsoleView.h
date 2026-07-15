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
};
