#pragma once

#include <filesystem>

#include "JsonDocument.h"

namespace sampleorder {

class JsonStore {
public:
    explicit JsonStore(std::filesystem::path file);

    // 파일이 없으면 {"samples":[], "orders":[]} 로 생성 후 저장, 있으면 그대로 로드
    void ensureLoaded();
    void save();

    nlohmann::ordered_json& samples();
    nlohmann::ordered_json& orders();

private:
    JsonDocument doc_;
    std::filesystem::path file_;
    nlohmann::ordered_json cache_;
};

} // namespace sampleorder
