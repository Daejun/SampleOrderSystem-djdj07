#include "ProductionQueue.h"

#include <cmath>

#include "OrderJson.h"

namespace {

std::int64_t toEpochSeconds(std::chrono::system_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}

} // namespace

ProductionQueue::ProductionQueue(sampleorder::JsonStore& store, SampleRepository& sampleRepository,
                                  OrderRepository& orderRepository, Clock clock)
    : store_(store),
      sampleRepository_(sampleRepository),
      orderRepository_(orderRepository),
      clock_(std::move(clock)) {}

void ProductionQueue::advance() {
    const std::int64_t now = toEpochSeconds(clock_());

    // 1) 활성 생산이 완료 시각을 지났으면: 재고 batch 반영 + CONFIRMED 전환.
    for (auto& item : store_.orders()) {
        Order order = orderFromJson(item);
        if (order.status != OrderStatus::PRODUCING || !order.productionStartedAtEpochSec.has_value()) {
            continue;
        }
        if (!order.productionCompletesAtEpochSec.has_value() || now < *order.productionCompletesAtEpochSec) {
            continue;
        }

        const auto sample = sampleRepository_.find(order.sampleId);
        sampleRepository_.adjustStockInMemory(order.sampleId, sample->stock + order.productionQuantity);

        order.status = OrderStatus::CONFIRMED;
        item = orderToJson(order);
        store_.save();
        break;  // 활성 생산은 최대 1건이므로 완료 처리는 한 건이면 충분
    }

    if (activeProduction().has_value()) {
        return;  // 활성 생산이 여전히 있으면(막 완료 처리했거나 아직 시간이 안 됨) 다음 시작은 없음
    }

    // 2) 활성 생산이 없으면(막 완료했거나 원래 없었음) 대기 큐 맨 앞 주문의 생산을 시작.
    for (auto& item : store_.orders()) {
        Order order = orderFromJson(item);
        if (order.status != OrderStatus::PRODUCING || order.productionStartedAtEpochSec.has_value()) {
            continue;
        }

        const auto sample = sampleRepository_.find(order.sampleId);
        order.productionQuantity =
            static_cast<int>(std::ceil(static_cast<double>(order.shortageQuantity) / sample->yield));
        order.productionStartedAtEpochSec = now;
        order.productionCompletesAtEpochSec =
            now + static_cast<std::int64_t>(sample->avgProductionTimeMinutes * 60.0 * order.productionQuantity);

        item = orderToJson(order);
        store_.save();
        break;  // 한 번의 advance() 호출에서는 최대 한 단계(생산 시작)만 진행
    }
}

std::vector<Order> ProductionQueue::waitingQueue() const {
    std::vector<Order> result;
    for (const auto& order : orderRepository_.listByStatus(OrderStatus::PRODUCING)) {
        if (!order.productionStartedAtEpochSec.has_value()) {
            result.push_back(order);
        }
    }
    return result;
}

std::optional<Order> ProductionQueue::activeProduction() const {
    for (const auto& order : orderRepository_.listByStatus(OrderStatus::PRODUCING)) {
        if (order.productionStartedAtEpochSec.has_value()) {
            return order;
        }
    }
    return std::nullopt;
}
