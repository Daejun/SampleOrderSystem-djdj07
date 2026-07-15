#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "Order.h"
#include "SampleRepository.h"
#include "../persistence/JsonStore.h"

class OrderRepository {
public:
    using Clock = std::function<std::chrono::system_clock::time_point()>;

    struct ReserveResult {
        bool success;
        std::string errorMessage;
        std::optional<Order> order;
    };

    OrderRepository(sampleorder::JsonStore& store, SampleRepository& sampleRepository,
                     Clock clock = &std::chrono::system_clock::now);

    // 실패 사유: 존재하지 않는 sampleId, quantity <= 0, customerName 빈 문자열
    ReserveResult reserve(const std::string& sampleId, const std::string& customerName, int quantity);

    std::optional<Order> find(const std::string& orderNumber) const;
    std::vector<Order> list() const;

private:
    std::string generateOrderNumber() const;

    sampleorder::JsonStore& store_;
    SampleRepository& sampleRepository_;
    Clock clock_;
};
