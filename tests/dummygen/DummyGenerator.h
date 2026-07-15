#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace dummygen {

struct RandomOptions {
    uint32_t seed = 0;
    int depth = 2;
    int count = 3;
};

// 텍스트 그대로 보관한다: malformed_json 케이스는 애초에 파싱이 안 되므로
// nlohmann::ordered_json으로 표현할 수 없다.
struct EdgeCase {
    std::string name;
    std::string content;
};

class DummyGenerator {
public:
    // depth 0 이하는 leaf(스칼라)만, depth N은 object가 "children" 배열(count개)을
    // depth-1로 재귀 포함해 실제로 N단계까지 중첩한다.
    static nlohmann::ordered_json generateRandom(const RandomOptions& options);

    // SchemaValidator 규칙(type/required/properties/items/pattern/minimum·maximum)을
    // 모두 만족하는 인스턴스를 생성한다. pattern은 몇 가지 후보 문자열 중 매칭되는
    // 것을 고르는 방식으로만 지원한다(임의 정규식 역생성은 지원하지 않음).
    static nlohmann::ordered_json generateValidFromSchema(const nlohmann::ordered_json& schema);

    // 유효한 인스턴스에서 스키마 규칙을 하나씩만 위반한 변형들을 생성한다.
    // 각 결과는 SchemaValidator::validate에서 정확히 그 규칙에 대한 오류를 낸다.
    static std::vector<nlohmann::ordered_json> generateInvalidFromSchema(const nlohmann::ordered_json& schema);

    // 엣지케이스/오류 테스트용 고정 시나리오 목록: 빈 객체/배열, 깊은 중첩,
    // 문법이 깨진 JSON, 배열 루트, 객체 루트.
    static std::vector<EdgeCase> generateEdgeCases();
};

} // namespace dummygen
