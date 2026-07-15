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

void ConsoleView::showProductionStatus(const std::optional<Order>& active, const std::vector<Order>& waiting) {
    if (active.has_value()) {
        std::printf("현재 생산 중: [%s] %s, 생산량 %d ea\n", active->orderNumber.c_str(),
                    active->sampleId.c_str(), active->productionQuantity);
    } else {
        std::printf("현재 생산 중인 주문이 없습니다.\n");
    }

    if (waiting.empty()) {
        std::printf("대기 중인 주문이 없습니다.\n");
        return;
    }
    std::printf("대기 큐 (FIFO):\n");
    for (const auto& order : waiting) {
        std::printf("  [%s] %s x %d ea (부족분 %d ea)\n", order.orderNumber.c_str(), order.sampleId.c_str(),
                    order.quantity, order.shortageQuantity);
    }
}

void ConsoleView::showOrderCountSummary(const OrderCountSummary& summary) {
    std::printf("RESERVED  %d건\n", summary.reserved);
    std::printf("CONFIRMED %d건\n", summary.confirmed);
    std::printf("PRODUCING %d건\n", summary.producing);
    std::printf("RELEASE   %d건\n", summary.release);
}

void ConsoleView::showInventoryStatus(const std::vector<SampleInventoryStatus>& statuses) {
    if (statuses.empty()) {
        std::printf("등록된 시료가 없습니다.\n");
        return;
    }
    for (const auto& status : statuses) {
        std::printf("[%s] %s 재고 %d ea (RESERVED 합 %d ea) - %s\n", status.sample.id.c_str(),
                    status.sample.name.c_str(), status.sample.stock, status.reservedQuantity,
                    inventoryLevelToString(status.level).c_str());
    }
}

void ConsoleView::showMainMenu(const MainMenuSummary& summary) {
    std::printf("\n=== 반도체 시료 생산주문관리 시스템 ===\n");
    std::printf("등록 시료 %d종  총 재고 %d ea  전체 주문 %d건  생산라인 대기 %d건\n", summary.sampleCount,
                summary.totalStock, summary.totalOrderCount, summary.producingCount);
    std::printf("[1] 시료 관리\n");
    std::printf("[2] 시료 주문\n");
    std::printf("[3] 주문 승인/거절\n");
    std::printf("[4] 모니터링\n");
    std::printf("[5] 생산 라인 조회\n");
    std::printf("[6] 출고 처리\n");
    std::printf("[0] 종료\n");
    std::printf("선택 > ");
}
