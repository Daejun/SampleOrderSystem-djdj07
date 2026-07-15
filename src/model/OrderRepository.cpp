#include "OrderRepository.h"

#include <iomanip>
#include <sstream>
#include <tuple>

#include "OrderJson.h"

namespace {

// Howard Hinnant의 civil_from_days 알고리즘: 1970-01-01 기준 일수 -> (년,월,일).
// gmtime_s/timegm 등 플랫폼별 CRT 함수에 의존하지 않고 UTC 기준으로 결정론적으로 계산한다.
std::tuple<int, int, int> civilFromDays(long long z) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long long y = static_cast<long long>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp + (mp < 10 ? 3 : static_cast<unsigned>(-9));
    return {static_cast<int>(y + (m <= 2 ? 1 : 0)), static_cast<int>(m), static_cast<int>(d)};
}

std::string formatDate(std::chrono::system_clock::time_point tp) {
    const long long totalSeconds =
        std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
    long long days = totalSeconds / 86400;
    if (totalSeconds % 86400 < 0) {
        --days;
    }

    const auto [year, month, day] = civilFromDays(days);
    std::ostringstream oss;
    oss << std::setw(4) << std::setfill('0') << year << std::setw(2) << std::setfill('0') << month
        << std::setw(2) << std::setfill('0') << day;
    return oss.str();
}

} // namespace

OrderRepository::OrderRepository(sampleorder::JsonStore& store, SampleRepository& sampleRepository, Clock clock)
    : store_(store), sampleRepository_(sampleRepository), clock_(std::move(clock)) {}

std::string OrderRepository::generateOrderNumber() const {
    const std::string prefix = "ORD-" + formatDate(clock_()) + "-";

    int countForDate = 0;
    for (const auto& item : store_.orders()) {
        const std::string orderNumber = item.at("orderNumber").get<std::string>();
        if (orderNumber.rfind(prefix, 0) == 0) {
            ++countForDate;
        }
    }

    std::ostringstream oss;
    oss << prefix << std::setw(4) << std::setfill('0') << (countForDate + 1);
    return oss.str();
}

OrderRepository::ReserveResult OrderRepository::reserve(const std::string& sampleId,
                                                         const std::string& customerName, int quantity) {
    if (!sampleRepository_.find(sampleId).has_value()) {
        return {false, "등록되지 않은 시료 ID입니다: " + sampleId, std::nullopt};
    }
    if (quantity <= 0) {
        return {false, "주문 수량은 0보다 커야 합니다.", std::nullopt};
    }
    if (customerName.empty()) {
        return {false, "고객명을 입력해야 합니다.", std::nullopt};
    }

    Order order;
    order.orderNumber = generateOrderNumber();
    order.sampleId = sampleId;
    order.customerName = customerName;
    order.quantity = quantity;
    order.status = OrderStatus::RESERVED;

    store_.orders().push_back(orderToJson(order));
    store_.save();

    return {true, "", order};
}

OrderRepository::ApproveResult OrderRepository::approve(const std::string& orderNumber) {
    for (auto& item : store_.orders()) {
        if (item.at("orderNumber").get<std::string>() != orderNumber) {
            continue;
        }

        Order order = orderFromJson(item);
        if (order.status != OrderStatus::RESERVED) {
            return {false, "이미 처리된 주문입니다: " + orderNumber, std::nullopt};
        }

        // reserve()에서 존재하는 sampleId만 받아들이므로 여기서는 항상 존재한다.
        const auto sample = sampleRepository_.find(order.sampleId);
        const int currentStock = sample->stock;

        if (currentStock >= order.quantity) {
            sampleRepository_.adjustStockInMemory(order.sampleId, currentStock - order.quantity);
            order.shortageQuantity = 0;
            order.status = OrderStatus::CONFIRMED;
        } else {
            order.shortageQuantity = order.quantity - currentStock;
            sampleRepository_.adjustStockInMemory(order.sampleId, 0);
            order.status = OrderStatus::PRODUCING;
        }

        item = orderToJson(order);
        store_.save();
        return {true, "", order};
    }
    return {false, "존재하지 않는 주문번호입니다: " + orderNumber, std::nullopt};
}

OrderRepository::RejectResult OrderRepository::reject(const std::string& orderNumber) {
    for (auto& item : store_.orders()) {
        if (item.at("orderNumber").get<std::string>() != orderNumber) {
            continue;
        }

        Order order = orderFromJson(item);
        if (order.status != OrderStatus::RESERVED) {
            return {false, "이미 처리된 주문입니다: " + orderNumber};
        }

        order.status = OrderStatus::REJECTED;
        item = orderToJson(order);
        store_.save();
        return {true, ""};
    }
    return {false, "존재하지 않는 주문번호입니다: " + orderNumber};
}

OrderRepository::ReleaseResult OrderRepository::release(const std::string& orderNumber) {
    for (auto& item : store_.orders()) {
        if (item.at("orderNumber").get<std::string>() != orderNumber) {
            continue;
        }

        Order order = orderFromJson(item);
        if (order.status != OrderStatus::CONFIRMED) {
            return {false, "출고 가능한(CONFIRMED) 상태가 아닙니다: " + orderNumber, std::nullopt};
        }

        order.status = OrderStatus::RELEASE;
        item = orderToJson(order);
        store_.save();
        return {true, "", order};
    }
    return {false, "존재하지 않는 주문번호입니다: " + orderNumber, std::nullopt};
}

std::optional<Order> OrderRepository::find(const std::string& orderNumber) const {
    for (const auto& item : store_.orders()) {
        if (item.at("orderNumber").get<std::string>() == orderNumber) {
            return orderFromJson(item);
        }
    }
    return std::nullopt;
}

std::vector<Order> OrderRepository::list() const {
    std::vector<Order> result;
    for (const auto& item : store_.orders()) {
        result.push_back(orderFromJson(item));
    }
    return result;
}

std::vector<Order> OrderRepository::listByStatus(OrderStatus status) const {
    std::vector<Order> result;
    for (const auto& item : store_.orders()) {
        Order order = orderFromJson(item);
        if (order.status == status) {
            result.push_back(order);
        }
    }
    return result;
}
