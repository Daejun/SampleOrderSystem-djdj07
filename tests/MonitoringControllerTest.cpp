#include <gtest/gtest.h>

#include <filesystem>

#include "StubView.h"
#include "controller/MonitoringController.h"
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

TEST(MonitoringControllerTest, ShowOrderSummaryForwardsToView) {
    const auto path = tempFile("controller_monitoring_order_summary.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    ASSERT_TRUE(sampleRepo.setStock("S-001", 500));
    OrderRepository orderRepo(store, sampleRepo);
    MonitoringService service(sampleRepo, orderRepo);
    StubView view;
    MonitoringController controller(service, view);

    orderRepo.reserve("S-001", "고객A", 10);
    const auto confirmed = orderRepo.reserve("S-001", "고객B", 20);
    orderRepo.approve(confirmed.order->orderNumber);

    controller.showOrderSummary();

    EXPECT_EQ(view.lastOrderCountSummary.reserved, 1);
    EXPECT_EQ(view.lastOrderCountSummary.confirmed, 1);
}

TEST(MonitoringControllerTest, ShowInventoryStatusForwardsToView) {
    const auto path = tempFile("controller_monitoring_inventory.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    OrderRepository orderRepo(store, sampleRepo);
    MonitoringService service(sampleRepo, orderRepo);
    StubView view;
    MonitoringController controller(service, view);

    controller.showInventoryStatus();

    ASSERT_EQ(view.lastInventoryStatus.size(), 1u);
    EXPECT_EQ(view.lastInventoryStatus[0].level, InventoryLevel::DEPLETED);
}
