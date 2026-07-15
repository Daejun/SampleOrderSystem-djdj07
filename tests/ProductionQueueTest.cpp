#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>

#include <nlohmann/json.hpp>

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

// ---------------------------------------------------------------------------
// 아래는 tc.md의 재고/타이밍 시나리오 매트릭스를 일반화한 테스트다. 각 테스트 상단 주석의
// TC ID는 tc.md의 해당 표 항목과 1:1로 대응된다.
// ---------------------------------------------------------------------------

// TC-1: 재고가 두 주문 합계보다 많으면(여유) 둘 다 생산 없이 즉시 재고로 충당된다.
TEST(ProductionQueueTest, TC1_SufficientStockCoversBothOrders_NoProductionForEither) {
    const auto path = tempFile("tc1_sufficient_both.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.2, 0.5, 0}).success);
    ASSERT_TRUE(sampleRepo.setStock("S-001", 200));
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);
    ProductionQueue queue(store, sampleRepo, orderRepo, testdata::fixedClock);

    const auto orderA = orderRepo.reserve("S-001", "고객A", 50);
    const auto approveA = orderRepo.approve(orderA.order->orderNumber);
    ASSERT_EQ(approveA.order->status, OrderStatus::CONFIRMED);
    EXPECT_EQ(sampleRepo.find("S-001")->stock, 150);

    const auto orderB = orderRepo.reserve("S-001", "고객B", 50);
    const auto approveB = orderRepo.approve(orderB.order->orderNumber);
    ASSERT_EQ(approveB.order->status, OrderStatus::CONFIRMED);
    EXPECT_EQ(sampleRepo.find("S-001")->stock, 100);

    queue.advance();
    EXPECT_FALSE(queue.activeProduction().has_value());
    EXPECT_TRUE(queue.waitingQueue().empty());
}

// TC-2: 재고가 첫 주문보다 적으면 부족분만큼만 생산량이 줄어들고(선반영), 두 번째 주문은 첫 생산이
// 활성 상태인 동안 대기 큐에만 머문다(활성 생산 1건 제한). 첫 생산 완료와 두 번째 생산 시작이
// 한 번의 advance() 호출 안에서 순차로 일어난다.
TEST(ProductionQueueTest, TC2_PartialStockReducesShortage_SecondOrderQueuesWhileFirstActive) {
    const auto path = tempFile("tc2_partial_stock.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.2, 0.5, 0}).success);
    ASSERT_TRUE(sampleRepo.setStock("S-001", 10));
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    MutableClock clock(testdata::fixedClock());
    ProductionQueue queue(store, sampleRepo, orderRepo, std::ref(clock));

    // t=0: 주문A(50) 승인 -> 재고 10 < 50 -> 부족분 40 (재고 전량 점유로 이미 0)
    const auto orderA = orderRepo.reserve("S-001", "고객A", 50);
    const auto approveA = orderRepo.approve(orderA.order->orderNumber);
    ASSERT_EQ(approveA.order->status, OrderStatus::PRODUCING);
    EXPECT_EQ(approveA.order->shortageQuantity, 40);
    EXPECT_EQ(sampleRepo.find("S-001")->stock, 0);

    queue.advance();  // 생산 시작: productionQuantity = ceil(40/0.5) = 80, 생산시간 = 0.2*80 = 16분(960초)
    const auto activeA = queue.activeProduction();
    ASSERT_TRUE(activeA.has_value());
    EXPECT_EQ(activeA->productionQuantity, 80);

    // t=5분: 주문B(50) 승인 -> 재고 0 < 50 -> 부족분 50, A가 아직 활성 상태이므로 B는 대기만 함
    clock.advanceBy(std::chrono::minutes(5));
    const auto orderB = orderRepo.reserve("S-001", "고객B", 50);
    const auto approveB = orderRepo.approve(orderB.order->orderNumber);
    ASSERT_EQ(approveB.order->status, OrderStatus::PRODUCING);
    EXPECT_EQ(approveB.order->shortageQuantity, 50);

    queue.advance();
    ASSERT_TRUE(queue.activeProduction().has_value());
    EXPECT_EQ(queue.activeProduction()->orderNumber, orderA.order->orderNumber);  // 여전히 A가 활성
    ASSERT_EQ(queue.waitingQueue().size(), 1u);
    EXPECT_EQ(queue.waitingQueue()[0].orderNumber, orderB.order->orderNumber);
    EXPECT_FALSE(queue.waitingQueue()[0].productionStartedAtEpochSec.has_value());

    // t=16분: A 완료 시각 도달 -> 완료 처리 + B 생산 시작이 한 번의 advance()에서 순차로 일어남
    clock.advanceBy(std::chrono::minutes(11));  // 총 16분 경과
    queue.advance();

    EXPECT_EQ(orderRepo.find(orderA.order->orderNumber)->status, OrderStatus::CONFIRMED);
    EXPECT_EQ(sampleRepo.find("S-001")->stock, 80);  // A의 productionQuantity(80) 전량 batch 반영

    const auto activeB = queue.activeProduction();
    ASSERT_TRUE(activeB.has_value());
    EXPECT_EQ(activeB->orderNumber, orderB.order->orderNumber);
    EXPECT_EQ(activeB->productionQuantity, 100);  // ceil(50/0.5) = 100 (A 완료로 재고가 늘어도 재계산되지 않음)
    EXPECT_TRUE(queue.waitingQueue().empty());
}

// TC-3: 두 번째 주문이 첫 주문의 생산이 "완료된 이후"에 도착하면, 그 시점의 최신 재고를 기준으로
// 독립적으로 평가된다 — 승인 시점에 재고가 이미 충분해졌다면 생산 없이 즉시 CONFIRMED될 수 있다.
TEST(ProductionQueueTest, TC3_OrderApprovedAfterPriorCompletion_EvaluatesAgainstUpdatedStockIndependently) {
    const auto path = tempFile("tc3_after_completion.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 0.2, 0.5, 0}).success);
    ASSERT_TRUE(sampleRepo.setStock("S-001", 10));
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    MutableClock clock(testdata::fixedClock());
    ProductionQueue queue(store, sampleRepo, orderRepo, std::ref(clock));

    const auto orderA = orderRepo.reserve("S-001", "고객A", 50);
    ASSERT_EQ(orderRepo.approve(orderA.order->orderNumber).order->status, OrderStatus::PRODUCING);
    queue.advance();  // 생산 시작 (완료까지 16분)

    clock.advanceBy(std::chrono::minutes(16));
    queue.advance();  // A 완료 -> 재고 80
    ASSERT_EQ(sampleRepo.find("S-001")->stock, 80);
    ASSERT_FALSE(queue.activeProduction().has_value());

    // t=20분: 주문C(30)가 A 완료 이후 도착. 재고 80 >= 30 이므로 생산 없이 즉시 충당된다.
    clock.advanceBy(std::chrono::minutes(4));
    const auto orderC = orderRepo.reserve("S-001", "고객C", 30);
    const auto approveC = orderRepo.approve(orderC.order->orderNumber);

    ASSERT_EQ(approveC.order->status, OrderStatus::CONFIRMED);
    EXPECT_EQ(approveC.order->shortageQuantity, 0);
    EXPECT_EQ(sampleRepo.find("S-001")->stock, 50);  // 80 - 30

    queue.advance();
    EXPECT_FALSE(queue.activeProduction().has_value());
    EXPECT_TRUE(queue.waitingQueue().empty());
}

// TC-4: 대기 주문이 3건이어도 병합 없이 FIFO 순서로 하나씩만 순차 처리된다.
TEST(ProductionQueueTest, TC4_ThreeOrdersMaintainStrictFifoOrderWithoutMerging) {
    const auto path = tempFile("tc4_three_orders_fifo.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 1.0, 1.0, 0}).success);
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    const auto first = orderRepo.reserve("S-001", "고객A", 5);
    const auto second = orderRepo.reserve("S-001", "고객B", 10);
    const auto third = orderRepo.reserve("S-001", "고객C", 15);
    ASSERT_TRUE(orderRepo.approve(first.order->orderNumber).success);   // 5분=300초
    ASSERT_TRUE(orderRepo.approve(second.order->orderNumber).success);  // 10분=600초
    ASSERT_TRUE(orderRepo.approve(third.order->orderNumber).success);   // 15분=900초

    MutableClock clock(testdata::fixedClock());
    ProductionQueue queue(store, sampleRepo, orderRepo, std::ref(clock));

    queue.advance();  // 첫 번째 시작
    ASSERT_EQ(queue.activeProduction()->orderNumber, first.order->orderNumber);
    ASSERT_EQ(queue.waitingQueue().size(), 2u);
    EXPECT_EQ(queue.waitingQueue()[0].orderNumber, second.order->orderNumber);
    EXPECT_EQ(queue.waitingQueue()[1].orderNumber, third.order->orderNumber);

    clock.advanceBy(std::chrono::seconds(300));  // 첫 번째 완료
    queue.advance();
    EXPECT_EQ(orderRepo.find(first.order->orderNumber)->status, OrderStatus::CONFIRMED);
    ASSERT_EQ(queue.activeProduction()->orderNumber, second.order->orderNumber);  // 두 번째 시작
    ASSERT_EQ(queue.waitingQueue().size(), 1u);
    EXPECT_EQ(queue.waitingQueue()[0].orderNumber, third.order->orderNumber);

    clock.advanceBy(std::chrono::seconds(600));  // 두 번째 완료
    queue.advance();
    EXPECT_EQ(orderRepo.find(second.order->orderNumber)->status, OrderStatus::CONFIRMED);
    ASSERT_EQ(queue.activeProduction()->orderNumber, third.order->orderNumber);  // 세 번째 시작
    EXPECT_TRUE(queue.waitingQueue().empty());

    clock.advanceBy(std::chrono::seconds(900));  // 세 번째 완료
    queue.advance();
    EXPECT_EQ(orderRepo.find(third.order->orderNumber)->status, OrderStatus::CONFIRMED);
    EXPECT_FALSE(queue.activeProduction().has_value());
}

// TC-5: 부족분이 수율로 나누어떨어지지 않으면 ceil로 올림하여 생산량을 계산한다.
TEST(ProductionQueueTest, TC5_ProductionQuantityRoundsUpForNonDivisibleShortage) {
    const auto path = tempFile("tc5_ceil_rounding.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository sampleRepo(store);
    ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 1.0, 0.3, 0}).success);  // 수율 0.3
    OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

    const auto reserved = orderRepo.reserve("S-001", "고객A", 7);  // 부족분 7, 7/0.3=23.33...
    ASSERT_TRUE(orderRepo.approve(reserved.order->orderNumber).success);

    ProductionQueue queue(store, sampleRepo, orderRepo, testdata::fixedClock);
    queue.advance();

    const auto active = queue.activeProduction();
    ASSERT_TRUE(active.has_value());
    EXPECT_EQ(active->productionQuantity, 24);  // ceil(7/0.3) = 24 (23으로 내림하지 않음)
}

// TC-6: 주문이 접수(승인)되어 생산 중인 상태에서 프로그램이 종료되고, 실제 생산 완료 예정 시각이 지난
// 뒤에 재시작되면, 재시작 직후 1회의 조회만으로 생산 완료가 반영되어야 한다. API(find/list)뿐 아니라
// 디스크에 저장된 원본 JSON 파일 내용으로도 직접 확인해, "파일에 영속화된 상태"가 근거임을 검증한다.
TEST(ProductionQueueTest, TC6_RestartAfterCompletionTimePersistsConfirmedStateToRawJsonFile) {
    const auto path = tempFile("tc6_restart_persistence.json");
    std::string orderNumber;
    {
        sampleorder::JsonStore store(path);
        store.ensureLoaded();
        SampleRepository sampleRepo(store);
        ASSERT_TRUE(sampleRepo.registerSample({"S-001", "A", 1.0, 0.5, 0}).success);
        OrderRepository orderRepo(store, sampleRepo, testdata::fixedClock);

        const auto reserved = orderRepo.reserve("S-001", "고객A", 20);  // 부족분 20
        ASSERT_TRUE(orderRepo.approve(reserved.order->orderNumber).success);
        orderNumber = reserved.order->orderNumber;

        MutableClock clock(testdata::fixedClock());
        ProductionQueue queue(store, sampleRepo, orderRepo, std::ref(clock));
        queue.advance();  // 생산 시작 (productionQuantity=ceil(20/0.5)=40, 생산시간=40분)
        // 이 지점에서 프로그램이 종료되었다고 가정 (아래 블록을 벗어나며 store 소멸, 파일에는 저장된 상태만 남음)
    }

    // 원본 파일을 직접 열어, 프로세스 종료 시점에 저장된 상태가 "생산 중(미완료)"임을 먼저 확인한다.
    {
        std::ifstream in(path);
        ASSERT_TRUE(in.is_open());
        nlohmann::json raw;
        in >> raw;
        ASSERT_EQ(raw["orders"].size(), 1u);
        EXPECT_EQ(raw["orders"][0]["status"].get<std::string>(), "PRODUCING");
        EXPECT_TRUE(raw["orders"][0].contains("productionCompletesAtEpochSec"));
        EXPECT_FALSE(raw["orders"][0]["productionCompletesAtEpochSec"].is_null());
    }

    // "재시작": 완료 예정 시각을 한참 지난 시각을 주입한 새 프로세스 상당 객체들로 파일을 다시 로드한다.
    const auto muchLater = []() { return testdata::fixedClock() + std::chrono::hours(24); };

    sampleorder::JsonStore reloaded(path);
    reloaded.ensureLoaded();
    SampleRepository sampleRepo(reloaded);
    OrderRepository orderRepo(reloaded, sampleRepo, testdata::fixedClock);
    ProductionQueue queue(reloaded, sampleRepo, orderRepo, muchLater);

    queue.advance();  // 재시작 직후 1회 호출만으로 완료 처리

    // API를 통한 확인
    const auto found = orderRepo.find(orderNumber);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->status, OrderStatus::CONFIRMED);
    EXPECT_EQ(sampleRepo.find("S-001")->stock, 40);  // productionQuantity(40) 전량 batch 반영

    // 디스크에 저장된 원본 JSON 파일을 다시 직접 열어, 완료 상태가 실제로 영속화되었는지 확인한다
    // (API 결과가 메모리 캐시만 반영하고 파일 저장은 누락되는 회귀를 잡기 위함).
    std::ifstream in(path);
    ASSERT_TRUE(in.is_open());
    nlohmann::json raw;
    in >> raw;
    ASSERT_EQ(raw["orders"].size(), 1u);
    EXPECT_EQ(raw["orders"][0]["status"].get<std::string>(), "CONFIRMED");
    ASSERT_EQ(raw["samples"].size(), 1u);
    EXPECT_EQ(raw["samples"][0]["stock"].get<int>(), 40);
}
