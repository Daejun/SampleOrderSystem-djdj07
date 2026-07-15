#include <gtest/gtest.h>

#include <filesystem>

#include "SampleTestData.h"
#include "StubView.h"
#include "controller/SampleController.h"
#include "dummygen/DummyGenerator.h"
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

Sample dummySample(const std::string& id, const std::string& name) {
    auto sample = testdata::toSample(DummyGenerator::generateValidFromSchema(testdata::sampleSchema()));
    sample->id = id;
    sample->name = name;
    return *sample;
}

}  // namespace

TEST(SampleControllerTest, RegisterSampleShowsSuccessMessage) {
    const auto path = tempFile("controller_register.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository repo(store);
    StubView view;
    SampleController controller(repo, view);

    const auto sample = dummySample("S-001", "A");
    controller.registerSample(sample.id, sample.name, sample.avgProductionTimeMinutes, sample.yield);

    EXPECT_TRUE(view.lastError.empty());
    EXPECT_FALSE(view.lastMessage.empty());
}

TEST(SampleControllerTest, RegisterDuplicateShowsError) {
    const auto path = tempFile("controller_dup.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository repo(store);
    StubView view;
    SampleController controller(repo, view);

    const auto sample = dummySample("S-001", "A");
    controller.registerSample(sample.id, sample.name, sample.avgProductionTimeMinutes, sample.yield);
    controller.registerSample(sample.id, "B", sample.avgProductionTimeMinutes, sample.yield);

    EXPECT_FALSE(view.lastError.empty());
}

TEST(SampleControllerTest, RegisterInvalidYieldShowsError) {
    const auto path = tempFile("controller_invalid.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository repo(store);
    StubView view;
    SampleController controller(repo, view);

    const auto sample = dummySample("S-001", "A");
    controller.registerSample(sample.id, sample.name, sample.avgProductionTimeMinutes, 1.5 /* yield 범위 밖 */);

    EXPECT_FALSE(view.lastError.empty());
}

TEST(SampleControllerTest, ListSamplesForwardsToView) {
    const auto path = tempFile("controller_list.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository repo(store);
    StubView view;
    SampleController controller(repo, view);

    const auto sample = dummySample("S-001", "A");
    controller.registerSample(sample.id, sample.name, sample.avgProductionTimeMinutes, sample.yield);
    controller.listSamples();

    ASSERT_EQ(view.lastSamples.size(), 1u);
    EXPECT_EQ(view.lastSamples[0].id, "S-001");
}

TEST(SampleControllerTest, SearchSamplesForwardsToView) {
    const auto path = tempFile("controller_search.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository repo(store);
    StubView view;
    SampleController controller(repo, view);

    const auto sample = dummySample("S-001", "Silicon Wafer");
    controller.registerSample(sample.id, sample.name, sample.avgProductionTimeMinutes, sample.yield);
    controller.searchSamples("silicon");

    ASSERT_EQ(view.lastSamples.size(), 1u);
    EXPECT_EQ(view.lastSamples[0].id, "S-001");
}
