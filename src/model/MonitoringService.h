#pragma once

#include <string>
#include <vector>

#include "OrderRepository.h"
#include "Sample.h"
#include "SampleRepository.h"

struct OrderCountSummary {
    int reserved = 0;
    int confirmed = 0;
    int producing = 0;
    int release = 0;
    // REJECTED는 무효 주문이므로 집계에서 제외 (prd.md §4.5)
};

enum class InventoryLevel { PLENTY, LOW, DEPLETED };  // 여유/부족/고갈

std::string inventoryLevelToString(InventoryLevel level);

struct SampleInventoryStatus {
    Sample sample;
    int reservedQuantity;  // 해당 시료의 RESERVED 주문 quantity 합
    InventoryLevel level;
};

class MonitoringService {
public:
    MonitoringService(SampleRepository& sampleRepository, OrderRepository& orderRepository);

    OrderCountSummary orderCountSummary() const;
    std::vector<SampleInventoryStatus> inventoryStatus() const;  // SampleRepository::list() 순서 그대로

private:
    SampleRepository& sampleRepository_;
    OrderRepository& orderRepository_;
};
