#include "JsonStore.h"

namespace sampleorder {

namespace {

nlohmann::ordered_json emptySchema() {
    return nlohmann::ordered_json{
        {"samples", nlohmann::ordered_json::array()},
        {"orders", nlohmann::ordered_json::array()}
    };
}

void ensureArrayField(nlohmann::ordered_json& root, const char* key) {
    if (!root.contains(key) || !root[key].is_array()) {
        root[key] = nlohmann::ordered_json::array();
    }
}

} // namespace

JsonStore::JsonStore(std::filesystem::path file) : file_(std::move(file)) {}

void JsonStore::ensureLoaded() {
    if (!std::filesystem::exists(file_)) {
        if (file_.has_parent_path()) {
            std::filesystem::create_directories(file_.parent_path());
        }
        cache_ = emptySchema();
        save();
        return;
    }

    doc_.load(file_);
    cache_ = doc_.root();
    ensureArrayField(cache_, "samples");
    ensureArrayField(cache_, "orders");
}

nlohmann::ordered_json& JsonStore::samples() {
    return cache_["samples"];
}

nlohmann::ordered_json& JsonStore::orders() {
    return cache_["orders"];
}

void JsonStore::save() {
    doc_.mutate([this](nlohmann::ordered_json& root) {
        root = cache_;
    });
    doc_.save(file_);
}

} // namespace sampleorder
