#include "ConsoleView.h"

#include <cstdio>

void ConsoleView::showMessage(const std::string& message) {
    std::printf("%s\n", message.c_str());
}

void ConsoleView::showError(const std::string& message) {
    std::printf("Error: %s\n", message.c_str());
}

void ConsoleView::showSamples(const std::vector<Sample>& samples) {
    if (samples.empty()) {
        std::printf("등록된 시료가 없습니다.\n");
        return;
    }
    for (const auto& sample : samples) {
        showSample(sample);
    }
}

void ConsoleView::showSample(const Sample& sample) {
    std::printf("[%s] %s (생산시간 %.2f min/ea, 수율 %.2f, 재고 %d ea)\n",
                sample.id.c_str(), sample.name.c_str(), sample.avgProductionTimeMinutes,
                sample.yield, sample.stock);
}

void ConsoleView::showOrders(const std::vector<Order>& orders) {
    if (orders.empty()) {
        std::printf("등록된 주문이 없습니다.\n");
        return;
    }
    for (const auto& order : orders) {
        showOrder(order);
    }
}

void ConsoleView::showOrder(const Order& order) {
    std::printf("[%s] %s x %d ea - %s (%s)\n", order.orderNumber.c_str(), order.sampleId.c_str(),
                order.quantity, order.customerName.c_str(), orderStatusToString(order.status).c_str());
}
