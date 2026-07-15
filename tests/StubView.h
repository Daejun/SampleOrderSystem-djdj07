#pragma once

#include <optional>
#include <string>
#include <vector>

#include "view/IView.h"

class StubView : public IView {
public:
    void showMessage(const std::string& message) override { lastMessage = message; }
    void showError(const std::string& message) override { lastError = message; }
    void showSamples(const std::vector<Sample>& samples) override { lastSamples = samples; }
    void showSample(const Sample& sample) override { lastSample = sample; }
    void showOrders(const std::vector<Order>& orders) override { lastOrders = orders; }
    void showOrder(const Order& order) override { lastOrder = order; }
    void showProductionStatus(const std::optional<Order>& active, const std::vector<Order>& waiting) override {
        lastActiveProduction = active;
        lastWaitingQueue = waiting;
    }

    std::string lastMessage;
    std::string lastError;
    std::vector<Sample> lastSamples;
    Sample lastSample;
    std::vector<Order> lastOrders;
    Order lastOrder;
    std::optional<Order> lastActiveProduction;
    std::vector<Order> lastWaitingQueue;
};
