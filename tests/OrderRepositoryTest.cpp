#include <gtest/gtest.h>

#include <filesystem>

#include "OrderTestData.h"
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

TEST(OrderRepositoryTest, ReserveGeneratesOrderNumberWithFixedDate) {
    const auto path = tempFile("order_reserve.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);

    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    auto request = testdata::toOrderRequest(DummyGenerator::generateValidFromSchema(testdata::orderRequestSchema()));
    ASSERT_TRUE(request.has_value());

    const auto result = orderRepo.reserve("S-001", request->customerName, request->quantity);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.order.has_value());
    EXPECT_EQ(result.order->orderNumber, "ORD-20260416-0001");
    EXPECT_EQ(result.order->status, OrderStatus::RESERVED);
}

TEST(OrderRepositoryTest, OrderNumberSequenceIncrementsForSameDate) {
    const auto path = tempFile("order_sequence.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);

    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    const auto first = orderRepo.reserve("S-001", "고객A", 10);
    const auto second = orderRepo.reserve("S-001", "고객B", 20);

    ASSERT_TRUE(first.order.has_value());
    ASSERT_TRUE(second.order.has_value());
    EXPECT_EQ(first.order->orderNumber, "ORD-20260416-0001");
    EXPECT_EQ(second.order->orderNumber, "ORD-20260416-0002");
}

TEST(OrderRepositoryTest, RejectsUnregisteredSampleId) {
    const auto path = tempFile("order_missing_sample.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    const auto result = orderRepo.reserve("S-999", "고객A", 10);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.order.has_value());
}

TEST(OrderRepositoryTest, RejectsNonPositiveQuantityFromDummyInvalidSet) {
    const auto path = tempFile("order_invalid_quantity.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    auto schema = testdata::orderRequestSchema();
    auto invalids = DummyGenerator::generateInvalidFromSchema(schema);

    int quantityViolations = 0;
    for (const auto& invalidJson : invalids) {
        auto request = testdata::toOrderRequest(invalidJson);
        if (!request.has_value()) {
            continue;  // 타입/필수 필드 위반은 이 도메인 규칙과 무관 -> 변환 단계에서 이미 걸러짐
        }
        if (request->quantity > 0) {
            continue;  // 우리 도메인 규칙과 무관한 변형 (예: customerName 이슈)
        }

        ++quantityViolations;
        const auto result = orderRepo.reserve("S-001", request->customerName, request->quantity);
        EXPECT_FALSE(result.success) << "should reject: " << invalidJson.dump();
    }

    ASSERT_GT(quantityViolations, 0) << "더미 생성기가 quantity 범위 위반 케이스를 하나도 만들지 못함";
}

TEST(OrderRepositoryTest, RejectsEmptyCustomerName) {
    const auto path = tempFile("order_empty_customer.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    const auto result = orderRepo.reserve("S-001", "", 10);

    EXPECT_FALSE(result.success);
}

TEST(OrderRepositoryTest, ApproveWithSufficientStockDeductsAndConfirms) {
    const auto path = tempFile("order_approve_sufficient.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    ASSERT_TRUE(sampleRepo.setStock("S-001", 500));
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    const auto reserved = orderRepo.reserve("S-001", "고객A", 200);
    ASSERT_TRUE(reserved.success);

    const auto approved = orderRepo.approve(reserved.order->orderNumber);

    ASSERT_TRUE(approved.success);
    ASSERT_TRUE(approved.order.has_value());
    EXPECT_EQ(approved.order->status, OrderStatus::CONFIRMED);
    EXPECT_EQ(approved.order->shortageQuantity, 0);

    auto sample = sampleRepo.find("S-001");
    ASSERT_TRUE(sample.has_value());
    EXPECT_EQ(sample->stock, 300);  // 500 - 200
}

TEST(OrderRepositoryTest, ApproveWithInsufficientStockZeroesStockAndProduces) {
    const auto path = tempFile("order_approve_insufficient.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    ASSERT_TRUE(sampleRepo.setStock("S-001", 30));
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    const auto reserved = orderRepo.reserve("S-001", "고객A", 200);
    ASSERT_TRUE(reserved.success);

    const auto approved = orderRepo.approve(reserved.order->orderNumber);

    ASSERT_TRUE(approved.success);
    ASSERT_TRUE(approved.order.has_value());
    EXPECT_EQ(approved.order->status, OrderStatus::PRODUCING);
    EXPECT_EQ(approved.order->shortageQuantity, 170);  // 200 - 30

    auto sample = sampleRepo.find("S-001");
    ASSERT_TRUE(sample.has_value());
    EXPECT_EQ(sample->stock, 0);
}

TEST(OrderRepositoryTest, ApproveExactStockMatchConfirmsWithoutShortage) {
    const auto path = tempFile("order_approve_exact_match.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    ASSERT_TRUE(sampleRepo.setStock("S-001", 50));
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    const auto reserved = orderRepo.reserve("S-001", "고객A", 50);  // 재고와 주문량이 정확히 같음
    ASSERT_TRUE(reserved.success);

    const auto approved = orderRepo.approve(reserved.order->orderNumber);

    // 경계값(>=): 재고 == 주문량이면 "부족"이 아니라 "충분" 분기를 타서 생산 없이 즉시 CONFIRMED되어야 한다.
    ASSERT_TRUE(approved.success);
    EXPECT_EQ(approved.order->status, OrderStatus::CONFIRMED);
    EXPECT_EQ(approved.order->shortageQuantity, 0);

    auto sample = sampleRepo.find("S-001");
    ASSERT_TRUE(sample.has_value());
    EXPECT_EQ(sample->stock, 0);
}

TEST(OrderRepositoryTest, RejectTransitionsToRejectedWithoutStockChange) {
    const auto path = tempFile("order_reject.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    ASSERT_TRUE(sampleRepo.setStock("S-001", 500));
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    const auto reserved = orderRepo.reserve("S-001", "고객A", 200);
    ASSERT_TRUE(reserved.success);

    const auto rejected = orderRepo.reject(reserved.order->orderNumber);

    EXPECT_TRUE(rejected.success);
    auto found = orderRepo.find(reserved.order->orderNumber);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->status, OrderStatus::REJECTED);

    auto sample = sampleRepo.find("S-001");
    ASSERT_TRUE(sample.has_value());
    EXPECT_EQ(sample->stock, 500);  // 변경 없음
}

TEST(OrderRepositoryTest, RejectsReapprovalOfNonReservedOrder) {
    const auto path = tempFile("order_reapprove.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    ASSERT_TRUE(sampleRepo.setStock("S-001", 500));
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    const auto reserved = orderRepo.reserve("S-001", "고객A", 200);
    ASSERT_TRUE(orderRepo.approve(reserved.order->orderNumber).success);

    const auto secondApprove = orderRepo.approve(reserved.order->orderNumber);
    EXPECT_FALSE(secondApprove.success);

    const auto rejectAfterApprove = orderRepo.reject(reserved.order->orderNumber);
    EXPECT_FALSE(rejectAfterApprove.success);
}

TEST(OrderRepositoryTest, ApproveUnknownOrderNumberFails) {
    const auto path = tempFile("order_approve_unknown.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    const auto result = orderRepo.approve("ORD-NOTFOUND-0001");

    EXPECT_FALSE(result.success);
}

TEST(OrderRepositoryTest, ListByStatusReturnsOnlyMatchingOrders) {
    const auto path = tempFile("order_list_by_status.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    ASSERT_TRUE(sampleRepo.setStock("S-001", 500));
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    const auto first = orderRepo.reserve("S-001", "고객A", 10);
    const auto second = orderRepo.reserve("S-001", "고객B", 20);
    ASSERT_TRUE(orderRepo.approve(first.order->orderNumber).success);

    const auto reservedOnly = orderRepo.listByStatus(OrderStatus::RESERVED);
    ASSERT_EQ(reservedOnly.size(), 1u);
    EXPECT_EQ(reservedOnly[0].orderNumber, second.order->orderNumber);

    const auto confirmedOnly = orderRepo.listByStatus(OrderStatus::CONFIRMED);
    ASSERT_EQ(confirmedOnly.size(), 1u);
    EXPECT_EQ(confirmedOnly[0].orderNumber, first.order->orderNumber);
}

TEST(OrderRepositoryTest, ApprovalPersistsAcrossReload) {
    const auto path = tempFile("order_approve_persist.json");
    std::string orderNumber;
    {
        sampleorder::JsonStore store(path);
        store.ensureLoaded();
        SampleRepository sampleRepo(store);
        ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
        ASSERT_TRUE(sampleRepo.setStock("S-001", 30));
        OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

        const auto reserved = orderRepo.reserve("S-001", "고객A", 200);
        ASSERT_TRUE(reserved.success);
        orderNumber = reserved.order->orderNumber;
        ASSERT_TRUE(orderRepo.approve(orderNumber).success);
    }

    sampleorder::JsonStore reloaded(path);
    reloaded.ensureLoaded();
    SampleRepository sampleRepo(reloaded);
    OrderRepository orderRepo(reloaded, sampleRepo, testdata::fixedClock);

    const auto found = orderRepo.find(orderNumber);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->status, OrderStatus::PRODUCING);
    EXPECT_EQ(found->shortageQuantity, 170);

    const auto sample = sampleRepo.find("S-001");
    ASSERT_TRUE(sample.has_value());
    EXPECT_EQ(sample->stock, 0);
}

TEST(OrderRepositoryTest, PersistsAcrossReload) {
    const auto path = tempFile("order_persist.json");
    {
        sampleorder::JsonStore store(path);
        store.ensureLoaded();
        SampleRepository sampleRepo(store);
        ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
        OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);
        ASSERT_TRUE(orderRepo.reserve("S-001", "고객A", 10).success);
    }

    sampleorder::JsonStore reloaded(path);
    reloaded.ensureLoaded();
    SampleRepository sampleRepo(reloaded);
    OrderRepository orderRepo(reloaded, sampleRepo, testdata::fixedClock);

    const auto list = orderRepo.list();
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].status, OrderStatus::RESERVED);
    EXPECT_EQ(list[0].sampleId, "S-001");
}
