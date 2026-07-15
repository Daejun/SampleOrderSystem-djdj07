#include "DummyGenerator.h"

#include <algorithm>
#include <functional>
#include <random>
#include <regex>
#include <string>

namespace dummygen {

namespace {

using Json = nlohmann::ordered_json;
using Ptr = Json::json_pointer;

nlohmann::ordered_json buildLeaf(std::mt19937& rng) {
    std::uniform_int_distribution<int> pick(0, 3);
    switch (pick(rng)) {
        case 0: {
            std::uniform_int_distribution<int> num(0, 1000);
            return num(rng);
        }
        case 1:
            return "value-" + std::to_string(std::uniform_int_distribution<int>(0, 999)(rng));
        case 2:
            return static_cast<bool>(std::uniform_int_distribution<int>(0, 1)(rng));
        default:
            return nullptr;
    }
}

nlohmann::ordered_json buildNode(std::mt19937& rng, int depth, int count) {
    std::uniform_int_distribution<int> id(1, 100000);
    nlohmann::ordered_json node;
    node["id"] = id(rng);
    node["name"] = "item-" + std::to_string(id(rng));
    node["active"] = static_cast<bool>(std::uniform_int_distribution<int>(0, 1)(rng));
    node["value"] = buildLeaf(rng);

    if (depth > 0) {
        nlohmann::ordered_json children = nlohmann::ordered_json::array();
        for (int i = 0; i < count; ++i) {
            children.push_back(buildNode(rng, depth - 1, count));
        }
        node["children"] = std::move(children);
    } else {
        node["children"] = nlohmann::ordered_json::array();
    }

    return node;
}

std::string schemaType(const Json& schema) {
    if (schema.contains("type")) {
        return schema.at("type").get<std::string>();
    }
    if (schema.contains("properties")) return "object";
    if (schema.contains("items")) return "array";
    return "string";
}

std::string satisfyPattern(const std::string& pattern) {
    static const std::vector<std::string> candidates = {"sample", "Sample123", "12345", "sample_123", "ABCxyz789"};
    for (const auto& candidate : candidates) {
        try {
            if (std::regex_search(candidate, std::regex(pattern))) {
                return candidate;
            }
        } catch (const std::regex_error&) {
            break;
        }
    }
    return "sample";
}

Json wrongTypeValue(const std::string& type) {
    if (type == "object") return "not-an-object";
    if (type == "array") return "not-an-array";
    if (type == "string") return 12345;
    if (type == "number" || type == "integer") return "not-a-number";
    if (type == "boolean") return "not-a-boolean";
    if (type == "null") return "not-null";
    return "unexpected";
}

Json generateValid(const Json& schema) {
    std::string type = schemaType(schema);

    if (type == "object") {
        Json obj = Json::object();
        if (schema.contains("properties")) {
            for (auto it = schema.at("properties").begin(); it != schema.at("properties").end(); ++it) {
                obj[it.key()] = generateValid(it.value());
            }
        }
        return obj;
    }

    if (type == "array") {
        Json arr = Json::array();
        if (schema.contains("items")) {
            arr.push_back(generateValid(schema.at("items")));
            arr.push_back(generateValid(schema.at("items")));
        } else {
            arr.push_back("sample-0");
            arr.push_back("sample-1");
        }
        return arr;
    }

    if (type == "string") {
        if (schema.contains("pattern")) {
            return satisfyPattern(schema.at("pattern").get<std::string>());
        }
        return "sample";
    }

    if (type == "number" || type == "integer") {
        double value = 1.0;
        if (schema.contains("minimum")) value = std::max(value, schema.at("minimum").get<double>());
        if (schema.contains("maximum")) value = std::min(value, schema.at("maximum").get<double>());
        if (type == "integer") return static_cast<long long>(value);
        return value;
    }

    if (type == "boolean") return true;
    if (type == "null") return nullptr;
    return "sample";
}

struct Mutation {
    std::string label;
    std::function<void(Json&)> apply;
};

void collectMutations(const Json& schema, const Ptr& ptr, std::vector<Mutation>& out) {
    if (schema.contains("type")) {
        std::string type = schema.at("type").get<std::string>();
        out.push_back({"타입 불일치: " + ptr.to_string(),
                        [ptr, type](Json& root) { root[ptr] = wrongTypeValue(type); }});
    }

    if (schema.contains("required")) {
        for (const auto& field : schema.at("required")) {
            std::string name = field.get<std::string>();
            out.push_back({"필수 필드 누락: " + ptr.to_string() + "/" + name, [ptr, name](Json& root) {
                               Json& node = root[ptr];
                               if (node.is_object() && node.contains(name)) {
                                   node.erase(name);
                               }
                           }});
        }
    }

    if (schema.contains("properties")) {
        for (auto it = schema.at("properties").begin(); it != schema.at("properties").end(); ++it) {
            collectMutations(it.value(), ptr / it.key(), out);
        }
    }

    if (schema.contains("items")) {
        collectMutations(schema.at("items"), ptr / "0", out);
    }

    if (schema.contains("pattern")) {
        out.push_back({"패턴 불일치: " + ptr.to_string(),
                        [ptr](Json& root) { root[ptr] = "!!!non-matching###"; }});
    }

    if (schema.contains("minimum")) {
        double minimum = schema.at("minimum").get<double>();
        out.push_back({"최솟값 미만: " + ptr.to_string(),
                        [ptr, minimum](Json& root) { root[ptr] = minimum - 1; }});
    }

    if (schema.contains("maximum")) {
        double maximum = schema.at("maximum").get<double>();
        out.push_back({"최댓값 초과: " + ptr.to_string(),
                        [ptr, maximum](Json& root) { root[ptr] = maximum + 1; }});
    }
}

Json buildDeepNesting(int depth) {
    Json node = "leaf";
    for (int i = 0; i < depth; ++i) {
        Json wrapper;
        wrapper["nested"] = std::move(node);
        node = std::move(wrapper);
    }
    return node;
}

} // namespace

nlohmann::ordered_json DummyGenerator::generateRandom(const RandomOptions& options) {
    std::mt19937 rng(options.seed);
    int depth = options.depth > 0 ? options.depth : 0;
    int count = options.count > 0 ? options.count : 0;

    nlohmann::ordered_json root;
    nlohmann::ordered_json items = nlohmann::ordered_json::array();
    for (int i = 0; i < count; ++i) {
        items.push_back(buildNode(rng, depth, count));
    }
    root["items"] = std::move(items);
    return root;
}

nlohmann::ordered_json DummyGenerator::generateValidFromSchema(const nlohmann::ordered_json& schema) {
    return generateValid(schema);
}

std::vector<nlohmann::ordered_json> DummyGenerator::generateInvalidFromSchema(const nlohmann::ordered_json& schema) {
    Json valid = generateValid(schema);

    std::vector<Mutation> mutations;
    collectMutations(schema, Ptr(""), mutations);

    std::vector<Json> invalids;
    invalids.reserve(mutations.size());
    for (const auto& mutation : mutations) {
        Json copy = valid;
        mutation.apply(copy);
        invalids.push_back(std::move(copy));
    }
    return invalids;
}

std::vector<EdgeCase> DummyGenerator::generateEdgeCases() {
    constexpr int kDeepNestingDepth = 60;
    constexpr int kIndentWidth = 2;

    std::vector<EdgeCase> cases;
    cases.push_back({"empty_object", Json::object().dump(kIndentWidth)});
    cases.push_back({"empty_array", Json::array().dump(kIndentWidth)});
    cases.push_back({"deep_nesting", buildDeepNesting(kDeepNestingDepth).dump(kIndentWidth)});
    cases.push_back({"malformed_json", "{ \"key\": \"unterminated string, missing closing brace"});
    cases.push_back({"array_root", Json::parse(R"(["a", 1, true, null, {"nested": 1}])").dump(kIndentWidth)});
    cases.push_back({"object_root", Json::parse(R"({"key": "value", "count": 1})").dump(kIndentWidth)});
    return cases;
}

} // namespace dummygen
