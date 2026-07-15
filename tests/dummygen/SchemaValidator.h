#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace dummygen {

struct ValidationError {
    std::string path;
    std::string message;
};

class SchemaValidator {
public:
    explicit SchemaValidator(nlohmann::ordered_json schema);

    std::vector<ValidationError> validate(const nlohmann::ordered_json& data) const;

private:
    nlohmann::ordered_json schema_;
};

} // namespace dummygen
