#include <gtest/gtest.h>

#include "view/ConsoleView.h"

TEST(ConsoleViewTest, ShowMessagePrintsMessage) {
    ConsoleView view;

    testing::internal::CaptureStdout();
    view.showMessage("hello");
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("hello"), std::string::npos);
}

TEST(ConsoleViewTest, ShowErrorPrintsErrorPrefixedMessage) {
    ConsoleView view;

    testing::internal::CaptureStdout();
    view.showError("something went wrong");
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("Error"), std::string::npos);
    EXPECT_NE(output.find("something went wrong"), std::string::npos);
}

TEST(ConsoleViewTest, ShowSamplesPrintsEachSample) {
    ConsoleView view;
    std::vector<Sample> samples{{"S-001", "실리콘", 0.5, 0.9, 100}, {"S-002", "질화갈륨", 0.3, 0.8, 50}};

    testing::internal::CaptureStdout();
    view.showSamples(samples);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("S-001"), std::string::npos);
    EXPECT_NE(output.find("실리콘"), std::string::npos);
    EXPECT_NE(output.find("100"), std::string::npos);
    EXPECT_NE(output.find("S-002"), std::string::npos);
    EXPECT_NE(output.find("질화갈륨"), std::string::npos);
    EXPECT_NE(output.find("50"), std::string::npos);
}

TEST(ConsoleViewTest, ShowSamplesPrintsMessageWhenEmpty) {
    ConsoleView view;

    testing::internal::CaptureStdout();
    view.showSamples({});
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("등록된 시료가 없습니다"), std::string::npos);
}

TEST(ConsoleViewTest, ShowSamplePrintsSampleDetail) {
    ConsoleView view;
    Sample sample{"S-001", "실리콘", 0.5, 0.9, 100};

    testing::internal::CaptureStdout();
    view.showSample(sample);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("S-001"), std::string::npos);
    EXPECT_NE(output.find("실리콘"), std::string::npos);
    EXPECT_NE(output.find("0.50"), std::string::npos);
    EXPECT_NE(output.find("0.90"), std::string::npos);
    EXPECT_NE(output.find("100"), std::string::npos);
}

TEST(ConsoleViewTest, ShowOrdersPrintsEachOrder) {
    ConsoleView view;
    Order first;
    first.orderNumber = "ORD-0001";
    first.sampleId = "S-001";
    first.quantity = 10;
    first.customerName = "고객A";
    first.status = OrderStatus::RESERVED;
    Order second;
    second.orderNumber = "ORD-0002";
    second.sampleId = "S-002";
    second.quantity = 20;
    second.customerName = "고객B";
    second.status = OrderStatus::CONFIRMED;

    testing::internal::CaptureStdout();
    view.showOrders({first, second});
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("ORD-0001"), std::string::npos);
    EXPECT_NE(output.find("고객A"), std::string::npos);
    EXPECT_NE(output.find("RESERVED"), std::string::npos);
    EXPECT_NE(output.find("ORD-0002"), std::string::npos);
    EXPECT_NE(output.find("고객B"), std::string::npos);
    EXPECT_NE(output.find("CONFIRMED"), std::string::npos);
}

TEST(ConsoleViewTest, ShowOrdersPrintsMessageWhenEmpty) {
    ConsoleView view;

    testing::internal::CaptureStdout();
    view.showOrders({});
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("등록된 주문이 없습니다"), std::string::npos);
}

TEST(ConsoleViewTest, ShowOrderPrintsOrderDetail) {
    ConsoleView view;
    Order order;
    order.orderNumber = "ORD-0001";
    order.sampleId = "S-001";
    order.quantity = 10;
    order.customerName = "고객A";
    order.status = OrderStatus::PRODUCING;

    testing::internal::CaptureStdout();
    view.showOrder(order);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("ORD-0001"), std::string::npos);
    EXPECT_NE(output.find("S-001"), std::string::npos);
    EXPECT_NE(output.find("10"), std::string::npos);
    EXPECT_NE(output.find("고객A"), std::string::npos);
    EXPECT_NE(output.find("PRODUCING"), std::string::npos);
}

TEST(ConsoleViewTest, ShowProductionStatusPrintsActiveAndWaiting) {
    ConsoleView view;
    Order active;
    active.orderNumber = "ORD-0001";
    active.sampleId = "S-001";
    active.productionQuantity = 40;
    Order waiting;
    waiting.orderNumber = "ORD-0002";
    waiting.sampleId = "S-002";
    waiting.quantity = 30;
    waiting.shortageQuantity = 25;

    testing::internal::CaptureStdout();
    view.showProductionStatus(active, {waiting});
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("ORD-0001"), std::string::npos);
    EXPECT_NE(output.find("40"), std::string::npos);
    EXPECT_NE(output.find("ORD-0002"), std::string::npos);
    EXPECT_NE(output.find("25"), std::string::npos);
}

TEST(ConsoleViewTest, ShowProductionStatusPrintsNoActiveMessageWhenIdle) {
    ConsoleView view;

    testing::internal::CaptureStdout();
    view.showProductionStatus(std::nullopt, {});
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("현재 생산 중인 주문이 없습니다"), std::string::npos);
}

TEST(ConsoleViewTest, ShowProductionStatusPrintsNoWaitingMessageWhenQueueEmpty) {
    ConsoleView view;

    testing::internal::CaptureStdout();
    view.showProductionStatus(std::nullopt, {});
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("대기 중인 주문이 없습니다"), std::string::npos);
}

TEST(ConsoleViewTest, ShowOrderCountSummaryPrintsAllFourCounts) {
    ConsoleView view;
    OrderCountSummary summary;
    summary.reserved = 1;
    summary.confirmed = 2;
    summary.producing = 3;
    summary.release = 4;

    testing::internal::CaptureStdout();
    view.showOrderCountSummary(summary);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("RESERVED"), std::string::npos);
    EXPECT_NE(output.find("CONFIRMED"), std::string::npos);
    EXPECT_NE(output.find("PRODUCING"), std::string::npos);
    EXPECT_NE(output.find("RELEASE"), std::string::npos);
    EXPECT_NE(output.find("1"), std::string::npos);
    EXPECT_NE(output.find("2"), std::string::npos);
    EXPECT_NE(output.find("3"), std::string::npos);
    EXPECT_NE(output.find("4"), std::string::npos);
    EXPECT_EQ(output.find("REJECTED"), std::string::npos);  // REJECTED 필드 자체가 없음
}

TEST(ConsoleViewTest, ShowInventoryStatusPrintsLevelForEachSample) {
    ConsoleView view;
    SampleInventoryStatus status{Sample{"S-001", "실리콘", 0.5, 0.9, 0}, 30, InventoryLevel::DEPLETED};

    testing::internal::CaptureStdout();
    view.showInventoryStatus({status});
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("S-001"), std::string::npos);
    EXPECT_NE(output.find("실리콘"), std::string::npos);
    EXPECT_NE(output.find("30"), std::string::npos);
    EXPECT_NE(output.find("고갈"), std::string::npos);
}

TEST(ConsoleViewTest, ShowInventoryStatusPrintsMessageWhenEmpty) {
    ConsoleView view;

    testing::internal::CaptureStdout();
    view.showInventoryStatus({});
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("등록된 시료가 없습니다"), std::string::npos);
}

TEST(ConsoleViewTest, ShowMainMenuPrintsSummaryAndMenuItems) {
    ConsoleView view;
    MainMenuSummary summary{3, 120, 7, 2};

    testing::internal::CaptureStdout();
    view.showMainMenu(summary);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("3"), std::string::npos);
    EXPECT_NE(output.find("120"), std::string::npos);
    EXPECT_NE(output.find("7"), std::string::npos);
    EXPECT_NE(output.find("2"), std::string::npos);
    EXPECT_NE(output.find("[1]"), std::string::npos);
    EXPECT_NE(output.find("[6]"), std::string::npos);
    EXPECT_NE(output.find("[0]"), std::string::npos);
}
