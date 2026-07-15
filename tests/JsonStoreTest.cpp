#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

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

TEST(JsonStoreTest, BootstrapsWhenFileMissing) {
    const auto path = tempFile("bootstrap_missing.json");

    sampleorder::JsonStore store(path);
    store.ensureLoaded();

    EXPECT_TRUE(std::filesystem::exists(path));
    EXPECT_TRUE(store.samples().is_array());
    EXPECT_TRUE(store.samples().empty());
    EXPECT_TRUE(store.orders().is_array());
    EXPECT_TRUE(store.orders().empty());
}

TEST(JsonStoreTest, LoadsExistingFile) {
    const auto path = tempFile("bootstrap_existing.json");
    {
        sampleorder::JsonStore seed(path);
        seed.ensureLoaded();
        seed.samples().push_back({{"id", "S-001"}});
        seed.save();
    }

    sampleorder::JsonStore store(path);
    store.ensureLoaded();

    ASSERT_EQ(store.samples().size(), 1u);
    EXPECT_EQ(store.samples()[0]["id"], "S-001");
}

// Phase 12: `!root.contains(key) || !root[key].is_array()`의 두 항을 각각 독립적으로 참으로 만드는 손상
// 파일을 직접 작성해 확인한다. BootstrapsWhenFileMissing과는 다르게 "파일 자체는 있지만 내용이 부족/잘못됨".
TEST(JsonStoreTest, RepairsMissingSamplesKeyInExistingFile) {
    const auto path = tempFile("repair_missing_key.json");
    {
        std::ofstream out(path);
        out << R"({"orders": []})";
    }

    sampleorder::JsonStore store(path);
    store.ensureLoaded();

    EXPECT_TRUE(store.samples().is_array());
    EXPECT_TRUE(store.samples().empty());
    EXPECT_TRUE(store.orders().is_array());
}

TEST(JsonStoreTest, RepairsWrongTypeSamplesFieldInExistingFile) {
    const auto path = tempFile("repair_wrong_type.json");
    {
        std::ofstream out(path);
        out << R"({"samples": "not-an-array", "orders": []})";
    }

    sampleorder::JsonStore store(path);
    store.ensureLoaded();

    EXPECT_TRUE(store.samples().is_array());
    EXPECT_TRUE(store.samples().empty());
}

TEST(JsonStoreTest, SaveThenReloadRoundTrips) {
    const auto path = tempFile("roundtrip.json");

    {
        sampleorder::JsonStore store(path);
        store.ensureLoaded();
        store.samples().push_back({{"id", "S-002"}, {"stock", 10}});
        store.save();
    }

    sampleorder::JsonStore reloaded(path);
    reloaded.ensureLoaded();

    ASSERT_EQ(reloaded.samples().size(), 1u);
    EXPECT_EQ(reloaded.samples()[0]["stock"], 10);
}
