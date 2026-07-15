#include <gtest/gtest.h>

#include <filesystem>

#include "SampleTestData.h"
#include "dummygen/DummyGenerator.h"
#include "dummygen/SchemaValidator.h"
#include "model/SampleRepository.h"
#include "persistence/JsonStore.h"

using dummygen::DummyGenerator;
using dummygen::SchemaValidator;

namespace {

std::filesystem::path tempFile(const std::string& name) {
    auto dir = std::filesystem::temp_directory_path() / "sample_order_tests";
    std::filesystem::create_directories(dir);
    auto path = dir / name;
    std::filesystem::remove(path);
    return path;
}

}  // namespace

TEST(SampleRepositoryTest, RegisterAndFindDummySample) {
    const auto path = tempFile("repo_register.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository repo(store);

    auto schema = testdata::sampleSchema();
    auto dummyJson = DummyGenerator::generateValidFromSchema(schema);
    ASSERT_TRUE(SchemaValidator(schema).validate(dummyJson).empty());

    auto sample = testdata::toSample(dummyJson);
    ASSERT_TRUE(sample.has_value());
    sample->id = "S-001";

    const auto result = repo.registerSample(*sample);
    EXPECT_TRUE(result.success);

    auto found = repo.find("S-001");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, sample->name);
    EXPECT_DOUBLE_EQ(found->avgProductionTimeMinutes, sample->avgProductionTimeMinutes);
    EXPECT_DOUBLE_EQ(found->yield, sample->yield);
    EXPECT_EQ(found->stock, 0);  // 등록 시 항상 0으로 강제
}

TEST(SampleRepositoryTest, RejectsDuplicateId) {
    const auto path = tempFile("repo_dup.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository repo(store);

    auto sample = testdata::toSample(DummyGenerator::generateValidFromSchema(testdata::sampleSchema()));
    ASSERT_TRUE(sample.has_value());
    sample->id = "S-001";

    ASSERT_TRUE(repo.registerSample(*sample).success);
    const auto result = repo.registerSample(*sample);

    EXPECT_FALSE(result.success);
}

TEST(SampleRepositoryTest, RejectsRangeViolationsFromDummyInvalidSet) {
    const auto path = tempFile("repo_invalid.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository repo(store);

    auto schema = testdata::sampleSchema();
    auto invalids = DummyGenerator::generateInvalidFromSchema(schema);
    SchemaValidator validator(schema);

    int rangeViolationCases = 0;
    for (const auto& invalidJson : invalids) {
        // 스키마 위반이 아닌 변형(예: 이 테스트와 무관한 필드 누락)은 건너뛴다.
        if (validator.validate(invalidJson).empty()) {
            continue;
        }

        auto sample = testdata::toSample(invalidJson);
        if (!sample.has_value()) {
            continue;  // 타입 불일치 변형은 Sample로 변환 자체가 불가능 -> 도메인 계층 이전에 걸러짐
        }
        sample->id = "S-range-" + std::to_string(rangeViolationCases);

        const bool violatesDomainRule =
            sample->avgProductionTimeMinutes <= 0.0 || sample->yield <= 0.0 || sample->yield > 1.0;
        if (!violatesDomainRule) {
            continue;  // 스키마상으로는 위반이지만 우리 도메인 규칙과는 무관한 변형(예: id 필드 이슈)
        }

        ++rangeViolationCases;
        const auto result = repo.registerSample(*sample);
        EXPECT_FALSE(result.success) << "should reject: " << invalidJson.dump();
    }

    ASSERT_GT(rangeViolationCases, 0) << "더미 생성기가 avgProductionTimeMinutes/yield 범위 위반 케이스를 하나도 만들지 못함";
}

TEST(SampleRepositoryTest, SearchMatchesPartialCaseInsensitive) {
    const auto path = tempFile("repo_search.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository repo(store);

    auto base = testdata::toSample(DummyGenerator::generateValidFromSchema(testdata::sampleSchema()));
    ASSERT_TRUE(base.has_value());

    Sample silicon = *base;
    silicon.id = "S-001";
    silicon.name = "Silicon Wafer";
    ASSERT_TRUE(repo.registerSample(silicon).success);

    Sample gan = *base;
    gan.id = "S-002";
    gan.name = "GaN Epitaxial";
    ASSERT_TRUE(repo.registerSample(gan).success);

    auto byName = repo.search("silicon");
    ASSERT_EQ(byName.size(), 1u);
    EXPECT_EQ(byName[0].id, "S-001");

    auto byId = repo.search("s-002");
    ASSERT_EQ(byId.size(), 1u);
    EXPECT_EQ(byId[0].id, "S-002");

    auto none = repo.search("nonexistent-keyword");
    EXPECT_TRUE(none.empty());
}

TEST(SampleRepositoryTest, SearchMatchesByProductionTimeAndYield) {
    const auto path = tempFile("repo_search_numeric.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository repo(store);

    // avgProductionTimeMinutes/yield 값이 서로 겹치지 않도록 구성해 교차 매치 오탐 없는지도 함께 확인
    ASSERT_TRUE(repo.registerSample({"S-101", "Alpha", 0.1234, 0.5, 0}).success);
    ASSERT_TRUE(repo.registerSample({"S-102", "Beta", 0.9, 0.8765, 0}).success);

    auto byProductionTime = repo.search("1234");  // "0.123400"의 일부
    ASSERT_EQ(byProductionTime.size(), 1u);
    EXPECT_EQ(byProductionTime[0].id, "S-101");

    auto byYield = repo.search("8765");  // "0.876500"의 일부
    ASSERT_EQ(byYield.size(), 1u);
    EXPECT_EQ(byYield[0].id, "S-102");
}

TEST(SampleRepositoryTest, SetStockUpdatesExistingSample) {
    const auto path = tempFile("repo_set_stock.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository repo(store);

    ASSERT_TRUE(repo.registerSample({"S-001", "A", 0.5, 0.9, 0}).success);

    EXPECT_TRUE(repo.setStock("S-001", 150));

    auto found = repo.find("S-001");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->stock, 150);
}

TEST(SampleRepositoryTest, SetStockReturnsFalseForUnknownId) {
    const auto path = tempFile("repo_set_stock_unknown.json");
    sampleorder::JsonStore store(path);
    store.ensureLoaded();
    SampleRepository repo(store);

    EXPECT_FALSE(repo.setStock("S-999", 10));
}

TEST(SampleRepositoryTest, PersistsAcrossReload) {
    const auto path = tempFile("repo_persist.json");
    auto sample = testdata::toSample(DummyGenerator::generateValidFromSchema(testdata::sampleSchema()));
    ASSERT_TRUE(sample.has_value());
    sample->id = "S-001";

    {
        sampleorder::JsonStore store(path);
        store.ensureLoaded();
        SampleRepository repo(store);
        ASSERT_TRUE(repo.registerSample(*sample).success);
    }

    sampleorder::JsonStore reloaded(path);
    reloaded.ensureLoaded();
    SampleRepository repo(reloaded);
    auto list = repo.list();

    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].id, "S-001");
}
