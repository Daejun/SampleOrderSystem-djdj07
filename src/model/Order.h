#pragma once

#include <string>

enum class OrderStatus { RESERVED, REJECTED, PRODUCING, CONFIRMED, RELEASE };

struct Order {
    std::string orderNumber;  // 자동 생성 (사용자 입력 아님)
    std::string sampleId;
    std::string customerName;
    int quantity = 0;
    OrderStatus status = OrderStatus::RESERVED;
};

inline std::string orderStatusToString(OrderStatus status) {
    switch (status) {
        case OrderStatus::RESERVED: return "RESERVED";
        case OrderStatus::REJECTED: return "REJECTED";
        case OrderStatus::PRODUCING: return "PRODUCING";
        case OrderStatus::CONFIRMED: return "CONFIRMED";
        case OrderStatus::RELEASE: return "RELEASE";
    }
    return "RESERVED";
}

inline OrderStatus orderStatusFromString(const std::string& text) {
    if (text == "REJECTED") return OrderStatus::REJECTED;
    if (text == "PRODUCING") return OrderStatus::PRODUCING;
    if (text == "CONFIRMED") return OrderStatus::CONFIRMED;
    if (text == "RELEASE") return OrderStatus::RELEASE;
    return OrderStatus::RESERVED;
}
