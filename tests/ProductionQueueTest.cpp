#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <functional>

#include "OrderTestData.h"
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

TEST(ProductionQueueTest, ApprovedInsufficientOrdersStayInWaitingQueueFifo) {
    const auto path = tempFile("pq_waiting_fifo.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    const auto first = orderRepo.reserve("S-001", "고객A", 50);
    const auto second = orderRepo.reserve("S-001", "고객B", 80);
    ASSERT_TRUE(orderRepo.approve(first.order->orderNumber).success);
    ASSERT_TRUE(orderRepo.approve(second.order->orderNumber).success);

    ProductionQueue queue(store, sampleRepo, orderRepo, testdata::fixedClock);
    // advance()를 아직 호출하지 않았으므로 두 주문 모두 대기 상태(활성 생산 없음)
    const auto waiting = queue.waitingQueue();

    ASSERT_EQ(waiting.size(), 2u);
    EXPECT_EQ(waiting[0].orderNumber, first.order->orderNumber);
    EXPECT_EQ(waiting[1].orderNumber, second.order->orderNumber);
    EXPECT_FALSE(queue.activeProduction().has_value());
}

TEST(ProductionQueueTest, AdvanceStartsFirstWaitingOrderWhenNoActiveProduction) {
    const auto path = tempFile("pq_start.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 2.0, 0.5, 0}).success);
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    const auto reserved = orderRepo.reserve("S-001", "고객A", 100);
    ASSERT_TRUE(orderRepo.approve(reserved.order->orderNumber).success);  // shortage=100

    MutableClock clock(testdata::fixedClock());
    ProductionQueue queue(store, sampleRepo, orderRepo, std::ref(clock));

    queue.advance();

    const auto active = queue.activeProduction();
    ASSERT_TRUE(active.has_value());
    EXPECT_EQ(active->orderNumber, reserved.order->orderNumber);
    EXPECT_EQ(active->productionQuantity, 200);  // ceil(100 / 0.5)
    ASSERT_TRUE(active->productionStartedAtEpochSec.has_value());
    ASSERT_TRUE(active->productionCompletesAtEpochSec.has_value());
    // 총 생산시간 = 2.0 min/ea * 200 ea = 400분 = 24000초
    EXPECT_EQ(*active->productionCompletesAtEpochSec - *active->productionStartedAtEpochSec, 24000);
    EXPECT_TRUE(queue.waitingQueue().empty());
}

TEST(ProductionQueueTest, RemainsProducingBeforeCompletionAndConfirmsAfter) {
    const auto path = tempFile("pq_completion.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 1.0, 1.0, 0}).success);
    ASSERT_TRUE(sampleRepo.setStock("S-001", 10));  // registerSample은 재고를 항상 0으로 강제하므로 별도 설정
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    const auto reserved = orderRepo.reserve("S-001", "고객A", 60);  // 재고 10 -> 부족분 50
    ASSERT_TRUE(orderRepo.approve(reserved.order->orderNumber).success);

    MutableClock clock(testdata::fixedClock());
    ProductionQueue queue(store, sampleRepo, orderRepo, std::ref(clock));

    queue.advance();  // 생산 시작 (productionQuantity=50, 총생산시간=50min=3000초)
    ASSERT_TRUE(queue.activeProduction().has_value());

    // 완료 시각 직전
    clock.advanceBy(std::chrono::seconds(2999));
    queue.advance();

    const auto stillActive = queue.activeProduction();
    ASSERT_TRUE(stillActive.has_value());
    EXPECT_EQ(stillActive->orderNumber, reserved.order->orderNumber);

    const auto sampleBefore = sampleRepo.find("S-001");
    ASSERT_TRUE(sampleBefore.has_value());
    EXPECT_EQ(sampleBefore->stock, 0);  // 아직 생산 중이므로 재고 미반영 (batch 갱신 전)

    // 완료 시각 도달 (총 3000초 경과)
    clock.advanceBy(std::chrono::seconds(1));
    queue.advance();

    EXPECT_FALSE(queue.activeProduction().has_value());
    const auto confirmed = orderRepo.find(reserved.order->orderNumber);
    ASSERT_TRUE(confirmed.has_value());
    EXPECT_EQ(confirmed->status, OrderStatus::CONFIRMED);

    const auto sampleAfter = sampleRepo.find("S-001");
    ASSERT_TRUE(sampleAfter.has_value());
    EXPECT_EQ(sampleAfter->stock, 50);  // 0(부족분 전량 점유 후) + productionQuantity(50) 전량 batch 반영
}

TEST(ProductionQueueTest, ProcessesMultipleWaitingOrdersSequentiallyWithoutMerging) {
    const auto path = tempFile("pq_sequential.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 1.0, 1.0, 0}).success);
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    const auto first = orderRepo.reserve("S-001", "고객A", 10);
    const auto second = orderRepo.reserve("S-001", "고객B", 20);
    ASSERT_TRUE(orderRepo.approve(first.order->orderNumber).success);   // productionQuantity 10, 10분=600초
    ASSERT_TRUE(orderRepo.approve(second.order->orderNumber).success);  // productionQuantity 20, 20분=1200초

    MutableClock clock(testdata::fixedClock());
    ProductionQueue queue(store, sampleRepo, orderRepo, std::ref(clock));

    queue.advance();  // 첫 번째 주문 생산 시작
    ASSERT_TRUE(queue.activeProduction().has_value());
    EXPECT_EQ(queue.activeProduction()->orderNumber, first.order->orderNumber);
    ASSERT_EQ(queue.waitingQueue().size(), 1u);
    EXPECT_EQ(queue.waitingQueue()[0].orderNumber, second.order->orderNumber);

    clock.advanceBy(std::chrono::seconds(600));  // 첫 번째 완료 시각 도달
    queue.advance();  // 완료 처리 + 두 번째 시작이 한 번의 advance() 안에서 순차로 일어남

    EXPECT_EQ(orderRepo.find(first.order->orderNumber)->status, OrderStatus::CONFIRMED);

    const auto activeAfter = queue.activeProduction();
    ASSERT_TRUE(activeAfter.has_value());
    EXPECT_EQ(activeAfter->orderNumber, second.order->orderNumber);
    EXPECT_TRUE(queue.waitingQueue().empty());  // 병합 없이 개별 항목으로 순차 처리됨
}

TEST(ProductionQueueTest, CompletesImmediatelyOnRestartAfterCompletionTimePassed) {
    const auto path = tempFile("pq_restart.json");
    std::string orderNumber;
    {
        sampleorder::JsonStore store(path);
        store.ensureLoaded();
        SampleRepository sampleRepo(store);
        ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 1.0, 1.0, 0}).success);
        OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

        const auto reserved = orderRepo.reserve("S-001", "고객A", 30);
        ASSERT_TRUE(orderRepo.approve(reserved.order->orderNumber).success);
        orderNumber = reserved.order->orderNumber;

        MutableClock clock(testdata::fixedClock());
        ProductionQueue queue(store, sampleRepo, orderRepo, std::ref(clock));
        queue.advance();  // 생산 시작. 이 시점에 프로세스가 종료되었다고 가정 (store가 스코프를 벗어나며 저장된 상태만 남음)
    }

    // "재시작": 완료 예정 시각을 한참 지난 고정 시각을 주입한 새 Clock으로 큐를 재구성
    const auto muchLater = []() { return testdata::fixedClock() + std::chrono::hours(24); };

    sampleorder::JsonStore reloaded(path);
    reloaded.ensureLoaded();
    SampleRepository sampleRepo(reloaded);
    OrderRepository orderRepo(reloaded, sampleRepo, testdata::fixedClock);
    ProductionQueue queue(reloaded, sampleRepo, orderRepo, muchLater);

    queue.advance();  // 재시작 직후 1회 호출만으로 완료 처리 (프로세스 생명주기와 무관)

    const auto found = orderRepo.find(orderNumber);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->status, OrderStatus::CONFIRMED);

    const auto sample = sampleRepo.find("S-001");
    ASSERT_TRUE(sample.has_value());
    EXPECT_EQ(sample->stock, 30);  // productionQuantity = ceil(30 / 1.0) = 30
}
