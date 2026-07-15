#include "MonitoringService.h"

std::string inventoryLevelToString(InventoryLevel level) {
    switch (level) {
        case InventoryLevel::PLENTY: return "여유";
        case InventoryLevel::LOW: return "부족";
        case InventoryLevel::DEPLETED: return "고갈";
    }
    return "부족";
}

MonitoringService::MonitoringService(SampleRepository& sampleRepository, OrderRepository& orderRepository)
    : sampleRepository_(sampleRepository), orderRepository_(orderRepository) {}

OrderCountSummary MonitoringService::orderCountSummary() const {
    OrderCountSummary summary;
    for (const auto& order : orderRepository_.list()) {
        switch (order.status) {
            case OrderStatus::RESERVED: ++summary.reserved; break;
            case OrderStatus::CONFIRMED: ++summary.confirmed; break;
            case OrderStatus::PRODUCING: ++summary.producing; break;
            case OrderStatus::RELEASE: ++summary.release; break;
            case OrderStatus::REJECTED: break;  // 무효 주문, 집계 제외
        }
    }
    return summary;
}

std::vector<SampleInventoryStatus> MonitoringService::inventoryStatus() const {
    const auto reservedOrders = orderRepository_.listByStatus(OrderStatus::RESERVED);

    std::vector<SampleInventoryStatus> result;
    for (const auto& sample : sampleRepository_.list()) {
        int reservedQuantity = 0;
        for (const auto& order : reservedOrders) {
            if (order.sampleId == sample.id) {
                reservedQuantity += order.quantity;
            }
        }

        InventoryLevel level;
        if (sample.stock == 0) {
            level = InventoryLevel::DEPLETED;
        } else if (sample.stock >= reservedQuantity) {
            level = InventoryLevel::PLENTY;
        } else {
            level = InventoryLevel::LOW;
        }

        result.push_back({sample, reservedQuantity, level});
    }
    return result;
}
