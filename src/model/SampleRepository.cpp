#include "SampleRepository.h"

#include <algorithm>
#include <cctype>

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

Sample fromJson(const nlohmann::ordered_json& j) {
    Sample s;
    s.id = j.at("id").get<std::string>();
    s.name = j.at("name").get<std::string>();
    s.avgProductionTimeMinutes = j.at("avgProductionTimeMinutes").get<double>();
    s.yield = j.at("yield").get<double>();
    s.stock = j.at("stock").get<int>();
    return s;
}

nlohmann::ordered_json toJson(const Sample& s) {
    return nlohmann::ordered_json{
        {"id", s.id},
        {"name", s.name},
        {"avgProductionTimeMinutes", s.avgProductionTimeMinutes},
        {"yield", s.yield},
        {"stock", s.stock}
    };
}

} // namespace

SampleRepository::SampleRepository(sampleorder::JsonStore& store) : store_(store) {}

SampleRepository::RegisterResult SampleRepository::registerSample(const Sample& sample) {
    if (sample.avgProductionTimeMinutes <= 0.0) {
        return {false, "평균 생산시간은 0보다 커야 합니다."};
    }
    if (sample.yield <= 0.0 || sample.yield > 1.0) {
        return {false, "수율은 0보다 크고 1 이하이어야 합니다."};
    }
    if (find(sample.id).has_value()) {
        return {false, "이미 존재하는 시료 ID입니다: " + sample.id};
    }

    Sample toStore = sample;
    toStore.stock = 0;  // 등록 시 재고는 항상 0으로 시작 (Phase 5에서만 증가)
    store_.samples().push_back(toJson(toStore));
    store_.save();
    return {true, ""};
}

std::optional<Sample> SampleRepository::find(const std::string& id) const {
    for (const auto& item : store_.samples()) {
        if (item.at("id").get<std::string>() == id) {
            return fromJson(item);
        }
    }
    return std::nullopt;
}

std::vector<Sample> SampleRepository::list() const {
    std::vector<Sample> result;
    for (const auto& item : store_.samples()) {
        result.push_back(fromJson(item));
    }
    return result;
}

bool SampleRepository::setStock(const std::string& id, int newStock) {
    for (auto& item : store_.samples()) {
        if (item.at("id").get<std::string>() == id) {
            item["stock"] = newStock;
            store_.save();
            return true;
        }
    }
    return false;
}

std::vector<Sample> SampleRepository::search(const std::string& keyword) const {
    std::vector<Sample> result;
    for (const auto& item : store_.samples()) {
        Sample s = fromJson(item);
        const bool matched = containsIgnoreCase(s.id, keyword) ||
                              containsIgnoreCase(s.name, keyword) ||
                              containsIgnoreCase(std::to_string(s.avgProductionTimeMinutes), keyword) ||
                              containsIgnoreCase(std::to_string(s.yield), keyword);
        if (matched) {
            result.push_back(s);
        }
    }
    return result;
}
