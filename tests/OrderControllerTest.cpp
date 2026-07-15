#include <gtest/gtest.h>

#include <filesystem>

#include "OrderTestData.h"
#include "StubView.h"
#include "controller/OrderController.h"
#include "dummygen/DummyGenerator.h"
#include "model/OrderRepository.h"
#include "model/SampleRepository.h"
#include "persistence/JsonStore.h"

using dummygen::DummyGenerator;

namespace {

std::filesystem::path tempFile(const std::string& name) {
    auto dir = std::filesystem::temp_directory_path() / "sample_order_tests";
    std::filesystem::create_directories(dir);
    auto path = dir / name;
    std::filesystem::remove(path);
    return path;
}

}  // namespace

TEST(OrderControllerTest, ReserveOrderShowsSuccessMessage) {
    const auto path = tempFile("controller_order_register.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);
    StubView view;
    OrderController controller(orderRepo, view);

    auto request = testdata::toOrderRequest(DummyGenerator::generateValidFromSchema(testdata::orderRequestSchema()));
    ASSERT_TRUE(request.has_value());

    controller.reserveOrder("S-001", request->customerName, request->quantity);

    EXPECT_TRUE(view.lastError.empty());
    EXPECT_FALSE(view.lastMessage.empty());
}

TEST(OrderControllerTest, ReserveOrderForUnknownSampleShowsError) {
    const auto path = tempFile("controller_order_unknown.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);
    StubView view;
    OrderController controller(orderRepo, view);

    controller.reserveOrder("S-999", "고객A", 10);

    EXPECT_FALSE(view.lastError.empty());
}

TEST(OrderControllerTest, ListOrdersForwardsToView) {
    const auto path = tempFile("controller_order_list.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);
    StubView view;
    OrderController controller(orderRepo, view);

    controller.reserveOrder("S-001", "고객A", 10);
    controller.listOrders();

    ASSERT_EQ(view.lastOrders.size(), 1u);
    EXPECT_EQ(view.lastOrders[0].sampleId, "S-001");
}
