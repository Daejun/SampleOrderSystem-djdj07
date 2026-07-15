#include "SchemaValidator.h"

#include <regex>

namespace dummygen {

namespace {

constexpr const char* kRootPath = "$";

bool typeMatches(const nlohmann::ordered_json& value, const std::string& type) {
    if (type == "object") return value.is_object();
    if (type == "array") return value.is_array();
    if (type == "string") return value.is_string();
    if (type == "number") return value.is_number();
    if (type == "integer") return value.is_number_integer();
    if (type == "boolean") return value.is_boolean();
    if (type == "null") return value.is_null();
    return true;
}

void validateNode(const nlohmann::ordered_json& schema, const nlohmann::ordered_json& data, const std::string& path,
                   std::vector<ValidationError>& errors) {
    if (schema.contains("type") && !typeMatches(data, schema.at("type").get<std::string>())) {
        errors.push_back({path, "타입이 스키마와 일치하지 않습니다: " + schema.at("type").get<std::string>()});
    }

    if (schema.contains("required") && data.is_object()) {
        for (const auto& field : schema.at("required")) {
            std::string name = field.get<std::string>();
            if (!data.contains(name)) {
                errors.push_back({path + "." + name, "필수 필드가 없습니다"});
            }
        }
    }

    if (schema.contains("properties") && data.is_object()) {
        for (auto it = schema.at("properties").begin(); it != schema.at("properties").end(); ++it) {
            const std::string& name = it.key();
            if (!data.contains(name)) {
                continue;
            }
            validateNode(it.value(), data.at(name), path + "." + name, errors);
        }
    }

    if (schema.contains("items") && data.is_array()) {
        for (size_t i = 0; i < data.size(); ++i) {
            validateNode(schema.at("items"), data[i], path + "[" + std::to_string(i) + "]", errors);
        }
    }

    if (schema.contains("pattern") && data.is_string()) {
        std::string pattern = schema.at("pattern").get<std::string>();
        if (!std::regex_search(data.get<std::string>(), std::regex(pattern))) {
            errors.push_back({path, "패턴과 일치하지 않습니다: " + pattern});
        }
    }

    if (data.is_number()) {
        double value = data.get<double>();
        if (schema.contains("minimum") && value < schema.at("minimum").get<double>()) {
            errors.push_back({path, "최솟값(" + schema.at("minimum").dump() + ")보다 작습니다"});
        }
        if (schema.contains("maximum") && value > schema.at("maximum").get<double>()) {
            errors.push_back({path, "최댓값(" + schema.at("maximum").dump() + ")보다 큽니다"});
        }
    }
}

} // namespace

SchemaValidator::SchemaValidator(nlohmann::ordered_json schema) : schema_(std::move(schema)) {}

std::vector<ValidationError> SchemaValidator::validate(const nlohmann::ordered_json& data) const {
    std::vector<ValidationError> errors;
    validateNode(schema_, data, kRootPath, errors);
    return errors;
}

} // namespace dummygen
