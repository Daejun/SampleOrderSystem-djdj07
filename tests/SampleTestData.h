#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "model/Sample.h"

// DummyDataGenerator-djdj07(dummygen)로 시료(Sample) 테스트 데이터를 생성하기 위한 스키마와
// JSON <-> Sample 변환 헬퍼. 실제 난수/변형 생성은 tests/dummygen(이식된 PoC 코드)이 담당한다.
namespace testdata {

// avgProductionTimeMinutes는 0보다 커야 하고, yield는 (0,1] 범위여야 하는
// SampleRepository::registerSample의 도메인 규칙을 반영한 스키마.
inline nlohmann::ordered_json sampleSchema() {
    return nlohmann::ordered_json::parse(R"({
        "type": "object",
        "required": ["id", "name", "avgProductionTimeMinutes", "yield"],
        "properties": {
            "id": { "type": "string" },
            "name": { "type": "string" },
            "avgProductionTimeMinutes": { "type": "number", "minimum": 0.1 },
            "yield": { "type": "number", "minimum": 0.1, "maximum": 1 }
        }
    })");
}

// 스키마를 만족하는 JSON 인스턴스를 Sample로 변환한다. 타입이 맞지 않으면 nullopt.
inline std::optional<Sample> toSample(const nlohmann::ordered_json& json) {
    try {
        Sample sample;
        sample.id = json.at("id").get<std::string>();
        sample.name = json.at("name").get<std::string>();
        sample.avgProductionTimeMinutes = json.at("avgProductionTimeMinutes").get<double>();
        sample.yield = json.at("yield").get<double>();
        return sample;
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

} // namespace testdata
