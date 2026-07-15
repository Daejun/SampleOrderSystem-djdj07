#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <functional>

#include "OrderTestData.h"
#include "model/MonitoringService.h"
#include "model/OrderRepository.h"
#include "model/ProductionQueue.h"
#include "model/SampleRepository.h"
#include "persistence/JsonStore.h"

namespace {

std::filesystem::path tempFile(const std::string& name) {
    auto dir = std::filesystem::temp_directory_path() / "sample_order_tests";
    std::filesystem::create_directories(dir);
    auto path = dir / name;
    std::filesystem::remove(path);
    return path;
}

// 테스트 중 시각을 자유롭게 앞당길 수 있는 가변 Clock. std::ref로 감싸 ProductionQueue::Clock에 주입한다.
class MutableClock {
public:
    explicit MutableClock(std::chrono::system_clock::time_point initial) : current_(initial) {}

    std::chrono::system_clock::time_point operator()() const { return current_; }

    void advanceBy(std::chrono::seconds delta) { current_ += delta; }

private:
    std::chrono::system_clock::time_point current_;
};

}  // namespace

// Controller/std::cin 계층은 거치지 않고, Repository/Service를 직접 연결해 전체 주문 여정을
// 처음부터 끝까지 자동화 회귀 테스트로 남긴다. 콘솔 실행 로그(Demonstrability)는 이 테스트를
// 대체하지 않으며 별도로 확인한다 (docs/design/phase8.md).
TEST(EndToEndTest, FullOrderJourneyFromReservationToRelease) {
    const auto path = tempFile("e2e_full_journey.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();

    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "테스트 시료", 1.0, 0.5, 0}).success);

    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);
    MutableClock clock(testdata::fixedClock());
    ProductionQueue queue(store, sampleRepo, orderRepo, std::ref(clock));
    MonitoringService monitoring(sampleRepo, orderRepo);

    // 1) 접수 (재고 0)
    const auto reserved = orderRepo.reserve("S-001", "고객A", 20);
    ASSERT_TRUE(reserved.success);
    EXPECT_EQ(reserved.order->status, OrderStatus::RESERVED);

    // 2) 승인 -> 재고 부족(0 < 20) -> PRODUCING, 부족분 20
    const auto approved = orderRepo.approve(reserved.order->orderNumber);
    ASSERT_TRUE(approved.success);
    EXPECT_EQ(approved.order->status, OrderStatus::PRODUCING);
    EXPECT_EQ(approved.order->shortageQuantity, 20);

    // 3) 생산 시작: productionQuantity = ceil(20/0.5) = 40, 총 생산시간 = 1.0*40 = 40분(2400초)
    queue.advance();
    const auto active = queue.activeProduction();
    ASSERT_TRUE(active.has_value());
    EXPECT_EQ(active->productionQuantity, 40);

    // 4) 완료 전: 아직 PRODUCING, 재고 미반영
    clock.advanceBy(std::chrono::seconds(2399));
    queue.advance();
    EXPECT_EQ(orderRepo.find(reserved.order->orderNumber)->status, OrderStatus::PRODUCING);
    EXPECT_EQ(sampleRepo.find("S-001")->stock, 0);

    // 5) 완료: CONFIRMED 전환 + 재고 batch 반영
    clock.advanceBy(std::chrono::seconds(1));
    queue.advance();
    ASSERT_EQ(orderRepo.find(reserved.order->orderNumber)->status, OrderStatus::CONFIRMED);
    EXPECT_EQ(sampleRepo.find("S-001")->stock, 40);

    // 6) 출고: RELEASE 전환, 재고 변경 없음
    const auto released = orderRepo.release(reserved.order->orderNumber);
    ASSERT_TRUE(released.success);
    EXPECT_EQ(released.order->status, OrderStatus::RELEASE);
    EXPECT_EQ(sampleRepo.find("S-001")->stock, 40);

    // 7) 최종 집계 확인
    const auto orderSummary = monitoring.orderCountSummary();
    EXPECT_EQ(orderSummary.release, 1);
    EXPECT_EQ(orderSummary.reserved, 0);
    EXPECT_EQ(orderSummary.producing, 0);
    EXPECT_EQ(orderSummary.confirmed, 0);

    const auto inventory = monitoring.inventoryStatus();
    ASSERT_EQ(inventory.size(), 1u);
    EXPECT_EQ(inventory[0].sample.stock, 40);
    EXPECT_EQ(inventory[0].reservedQuantity, 0);
    EXPECT_EQ(inventory[0].level, InventoryLevel::PLENTY);  // 40 >= 0 (RESERVED 없음)

    const auto mainMenu = monitoring.mainMenuSummary();
    EXPECT_EQ(mainMenu.sampleCount, 1);
    EXPECT_EQ(mainMenu.totalStock, 40);
    EXPECT_EQ(mainMenu.totalOrderCount, 1);
    EXPECT_EQ(mainMenu.producingCount, 0);
}
