#include <gtest/gtest.h>

#include <filesystem>

#include "model/MonitoringService.h"
#include "model/OrderRepository.h"
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

}  // namespace

TEST(MonitoringServiceTest, OrderCountSummaryExcludesRejected) {
    const auto path = tempFile("monitoring_order_summary.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    ASSERT_TRUE(sampleRepo.setStock("S-001", 500));
    OrderRepository orderRepo(store, sampleRepo);

    const auto reserved = orderRepo.reserve("S-001", "고객A", 10);            // RESERVED로 남김
    const auto rejected = orderRepo.reserve("S-001", "고객B", 10);
    orderRepo.reject(rejected.order->orderNumber);                            // REJECTED (집계 제외 대상)
    const auto confirmed = orderRepo.reserve("S-001", "고객C", 10);
    orderRepo.approve(confirmed.order->orderNumber);                          // 재고 충분 -> CONFIRMED
    const auto producing = orderRepo.reserve("S-001", "고객D", 10000);
    orderRepo.approve(producing.order->orderNumber);                          // 재고 부족 -> PRODUCING (재고 0으로 소진)
    ASSERT_TRUE(sampleRepo.setStock("S-001", 500));                           // 재고 리셋 (다음 케이스가 CONFIRMED로 승인되도록)
    const auto released = orderRepo.reserve("S-001", "고객E", 5);
    ASSERT_EQ(orderRepo.approve(released.order->orderNumber).order->status, OrderStatus::CONFIRMED);
    ASSERT_TRUE(orderRepo.release(released.order->orderNumber).success);      // RELEASE
    (void)reserved;

    MonitoringService monitoring(sampleRepo, orderRepo);
    const auto summary = monitoring.orderCountSummary();

    EXPECT_EQ(summary.reserved, 1);
    EXPECT_EQ(summary.confirmed, 1);
    EXPECT_EQ(summary.producing, 1);
    EXPECT_EQ(summary.release, 1);
    // REJECTED 1건은 어느 카운트에도 포함되지 않아야 한다 (총합 4건, REJECTED까지 포함하면 5건).
}

TEST(MonitoringServiceTest, InventoryStatusDepletedWhenStockIsZeroRegardlessOfReserved) {
    const auto path = tempFile("monitoring_depleted.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    // 재고 0, RESERVED 주문도 없음 -> 그래도 "고갈"이어야 한다 (최우선 판정).
    OrderRepository orderRepo(store, sampleRepo);

    MonitoringService monitoring(sampleRepo, orderRepo);
    const auto statuses = monitoring.inventoryStatus();

    ASSERT_EQ(statuses.size(), 1u);
    EXPECT_EQ(statuses[0].level, InventoryLevel::DEPLETED);
    EXPECT_EQ(statuses[0].reservedQuantity, 0);
}

TEST(MonitoringServiceTest, InventoryStatusPlentyWhenStockEqualsReservedSum) {
    const auto path = tempFile("monitoring_plenty_boundary.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    ASSERT_TRUE(sampleRepo.setStock("S-001", 30));
    OrderRepository orderRepo(store, sampleRepo);

    orderRepo.reserve("S-001", "고객A", 10);
    orderRepo.reserve("S-001", "고객B", 20);  // RESERVED 합 = 30, 재고 = 30 (경계값)

    MonitoringService monitoring(sampleRepo, orderRepo);
    const auto statuses = monitoring.inventoryStatus();

    ASSERT_EQ(statuses.size(), 1u);
    EXPECT_EQ(statuses[0].reservedQuantity, 30);
    EXPECT_EQ(statuses[0].level, InventoryLevel::PLENTY);  // 경계값 포함 "여유"
}

TEST(MonitoringServiceTest, InventoryStatusLowWhenStockBelowReservedSum) {
    const auto path = tempFile("monitoring_low.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    ASSERT_TRUE(sampleRepo.setStock("S-001", 29));
    OrderRepository orderRepo(store, sampleRepo);

    orderRepo.reserve("S-001", "고객A", 10);
    orderRepo.reserve("S-001", "고객B", 20);  // RESERVED 합 = 30 > 재고 29

    MonitoringService monitoring(sampleRepo, orderRepo);
    const auto statuses = monitoring.inventoryStatus();

    ASSERT_EQ(statuses.size(), 1u);
    EXPECT_EQ(statuses[0].level, InventoryLevel::LOW);
}

TEST(MonitoringServiceTest, InventoryStatusIgnoresNonReservedOrdersInComparison) {
    const auto path = tempFile("monitoring_ignores_non_reserved.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    ASSERT_TRUE(sampleRepo.setStock("S-001", 100));
    OrderRepository orderRepo(store, sampleRepo);

    // 대량의 CONFIRMED/PRODUCING 주문이 있어도 RESERVED가 없으면 재고 판정 비교 대상은 0이어야 한다.
    const auto confirmed = orderRepo.reserve("S-001", "고객A", 40);
    orderRepo.approve(confirmed.order->orderNumber);  // 재고 100 -> 60으로 차감, CONFIRMED

    MonitoringService monitoring(sampleRepo, orderRepo);
    const auto statuses = monitoring.inventoryStatus();

    ASSERT_EQ(statuses.size(), 1u);
    EXPECT_EQ(statuses[0].reservedQuantity, 0);        // CONFIRMED는 비교 대상 아님
    EXPECT_EQ(statuses[0].sample.stock, 60);
    EXPECT_EQ(statuses[0].level, InventoryLevel::PLENTY);  // 60 >= 0
}

TEST(MonitoringServiceTest, InventoryLevelToStringMapsAllLevels) {
    EXPECT_EQ(inventoryLevelToString(InventoryLevel::PLENTY), "여유");
    EXPECT_EQ(inventoryLevelToString(InventoryLevel::LOW), "부족");
    EXPECT_EQ(inventoryLevelToString(InventoryLevel::DEPLETED), "고갈");
}
