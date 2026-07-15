#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <vector>

#include "Order.h"
#include "OrderRepository.h"
#include "SampleRepository.h"
#include "../persistence/JsonStore.h"

// 단일 생산 라인의 FIFO 생산 큐. 큐는 store_.orders() 배열 내 등장 순서로 표현되며
// 별도 병합 없이 개별 항목으로 순차 처리된다.
class ProductionQueue {
public:
    using Clock = std::function<std::chrono::system_clock::time_point()>;

    ProductionQueue(sampleorder::JsonStore& store, SampleRepository& sampleRepository,
                     OrderRepository& orderRepository, Clock clock = &std::chrono::system_clock::now);

    // 호출될 때마다 최대 한 단계만 진행한다:
    //   1) 활성 생산이 완료 시각을 지났으면: 재고에 productionQuantity를 batch로 더하고 CONFIRMED 전환.
    //   2) 활성 생산이 없으면(막 완료했거나 원래 없었음): 대기 큐 맨 앞 주문의 생산을 시작.
    // 완료 판정은 저장된 시각과 clock_()만 비교하므로 프로그램 생명주기와 무관하게 동작한다.
    void advance();

    std::vector<Order> waitingQueue() const;        // status==PRODUCING && !productionStartedAt (FIFO)
    std::optional<Order> activeProduction() const;  // status==PRODUCING && productionStartedAt 있음

private:
    sampleorder::JsonStore& store_;
    SampleRepository& sampleRepository_;
    OrderRepository& orderRepository_;
    Clock clock_;
};
