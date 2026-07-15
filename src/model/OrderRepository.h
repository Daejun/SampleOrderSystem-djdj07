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

    struct ApproveResult {
        bool success;
        std::string errorMessage;
        std::optional<Order> order;
    };

    struct RejectResult {
        bool success;
        std::string errorMessage;
    };

    OrderRepository(sampleorder::JsonStore& store, SampleRepository& sampleRepository,
                     Clock clock = &std::chrono::system_clock::now);

    // 실패 사유: 존재하지 않는 sampleId, quantity <= 0, customerName 빈 문자열
    ReserveResult reserve(const std::string& sampleId, const std::string& customerName, int quantity);

    // 실패 사유: 존재하지 않는 주문번호, 이미 RESERVED가 아닌 주문.
    // 재고 충분 -> 주문 수량만큼 차감 후 CONFIRMED. 재고 부족 -> 기존 재고 전량 차감(0) 후
    // shortageQuantity 계산, PRODUCING 전환.
    ApproveResult approve(const std::string& orderNumber);

    // 실패 사유: 존재하지 않는 주문번호, 이미 RESERVED가 아닌 주문. 재고는 변경하지 않는다.
    RejectResult reject(const std::string& orderNumber);

    std::optional<Order> find(const std::string& orderNumber) const;
    std::vector<Order> list() const;
    std::vector<Order> listByStatus(OrderStatus status) const;

private:
    std::string generateOrderNumber() const;

    sampleorder::JsonStore& store_;
    SampleRepository& sampleRepository_;
    Clock clock_;
};
